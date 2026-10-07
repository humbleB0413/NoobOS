#include "linux/vga.h"

#include "string.h"
#include "port_io.h"
#include "linux/spinlock.h"

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer = (uint16_t*)VGA_MEMORY;
static void (*terminal_mirror)(char) = 0;

/* CRTC 커서 위치 레지스터(0x0E/0x0F)를 갱신해 깜빡이는 하드웨어 커서를 출력 위치에 맞춘다 */
static void terminal_update_cursor(void) {
  uint16_t pos = (uint16_t)(terminal_row * VGA_WIDTH + terminal_column);
  outb(0x3D4, 0x0F);
  outb(0x3D5, (uint8_t)(pos & 0xFF));
  outb(0x3D4, 0x0E);
  outb(0x3D5, (uint8_t)(pos >> 8));
}

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
  return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
  return (uint16_t)uc | (uint16_t)color << 8;
}

void terminal_initialize(void) {
  terminal_row = 0;
  terminal_column = 0;
  terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      const size_t index = y * VGA_WIDTH + x;
      terminal_buffer[index] = vga_entry(' ', terminal_color);
    }
  }
  terminal_update_cursor();
}

void terminal_setcolor(uint8_t color) { terminal_color = color; }

/* VGA 로 나가는 모든 문자를 다른 출력 장치(시리얼 등)에도 복제한다 */
void terminal_set_mirror(void (*mirror)(char)) { terminal_mirror = mirror; }

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = vga_entry(c, color);
}

/* 한 줄 위로 밀어 올리고 마지막 줄을 비운다 — 25줄을 넘는 출력이 화면 밖(0xB8000+4000 이후)에 써지던 문제 수정 */
static void terminal_scroll(void) {
  memcpy(terminal_buffer, terminal_buffer + VGA_WIDTH,
         (VGA_HEIGHT - 1) * VGA_WIDTH * sizeof(uint16_t));
  for (size_t x = 0; x < VGA_WIDTH; x++) {
    terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
  }
  terminal_row = VGA_HEIGHT - 1;
}

static void terminal_newline(void) {
  terminal_column = 0;
  if (++terminal_row == VGA_HEIGHT) terminal_scroll();
}

void terminal_putchar(char c) {
  if (terminal_mirror) terminal_mirror(c);
  switch (c) {
    case '\n':
      terminal_newline();
      break;
    case '\t':
      terminal_column = (terminal_column + 4) & ~3u;
      if (terminal_column >= VGA_WIDTH) terminal_newline();
      break;
    case '\b':
      if (terminal_column > 0) {
        terminal_column--;
      } else if (terminal_row > 0) {
        terminal_row--;
        terminal_column = VGA_WIDTH - 1;
      }
      terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
      break;
    default:
      terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
      if (++terminal_column == VGA_WIDTH) terminal_newline();
  }
  terminal_update_cursor();
}

void terminal_write(const char* data, size_t size) {
  for (size_t i = 0; i < size; i++) terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
  terminal_write(data, strlen(data));
}

/* 숫자를 base 진법 문자열로 buf 에 역순 없이 채우고 길이를 돌려준다 (64비트 나눗셈은 libgcc 의 __udivdi3/__umoddi3) */
static int kformat_uint(char *buf, unsigned long long n, unsigned base, int uppercase)
{
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[64];
    int  len = 0, i = 0;

    do {
        tmp[len++] = digits[n % base];
        n /= base;
    } while (n > 0);
    while (len > 0)
        buf[i++] = tmp[--len];
    return i;
}

/* sign("-" 또는 "0x" 같은 접두어) + body 를 width 에 맞춰 출력. '0' 패딩은 접두어 뒤에 채운다 */
static void kput_field(const char *prefix, const char *body, int body_len,
                       int width, int left_align, int zero_pad)
{
    int prefix_len = prefix ? (int)strlen(prefix) : 0;
    int pad = width - prefix_len - body_len;

    if (!left_align && !zero_pad)
        while (pad-- > 0) terminal_putchar(' ');
    if (prefix)
        terminal_writestring(prefix);
    if (!left_align && zero_pad)
        while (pad-- > 0) terminal_putchar('0');
    for (int i = 0; i < body_len; i++)
        terminal_putchar(body[i]);
    if (left_align)
        while (pad-- > 0) terminal_putchar(' ');
}

/*
 * 지원: %d %i %u %x %X %o %b(2진수, 비표준) %c %s %p %%
 * 플래그 '-'(왼쪽 정렬) '0'(0 채움), 폭(숫자), 길이 지정자 l / ll(64비트)
 */
void kvprintf(const char *fmt, va_list ap)
{
    char buf[72];

    while (*fmt) {
        if (*fmt != '%') {
            terminal_putchar(*fmt++);
            continue;
        }
        fmt++; // skip '%'

        int left_align = 0, zero_pad = 0, width = 0;
        for (;; fmt++) {
            if (*fmt == '-') left_align = 1;
            else if (*fmt == '0') zero_pad = 1;
            else break;
        }
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');

        int is_long_long = 0;
        if (*fmt == 'l') {
            fmt++;
            if (*fmt == 'l') { fmt++; is_long_long = 1; }
        }

        unsigned base = 10;
        int uppercase = 0;

        switch (*fmt) {
        case 'd': case 'i': {
            long long val = is_long_long ? va_arg(ap, long long) : (long long)va_arg(ap, int);
            unsigned long long mag = val < 0 ? -(unsigned long long)val : (unsigned long long)val;
            int len = kformat_uint(buf, mag, 10, 0);
            kput_field(val < 0 ? "-" : 0, buf, len, width, left_align, zero_pad);
            break;
        }
        case 'X': uppercase = 1; /* fallthrough */
        case 'x': base = 16; goto unsigned_conv;
        case 'o': base = 8;  goto unsigned_conv;
        case 'b': base = 2;  goto unsigned_conv;   // 커널 디버깅용 binary (비표준이지만 유용)
        case 'u':
        unsigned_conv: {
            unsigned long long val = is_long_long ? va_arg(ap, unsigned long long)
                                                  : (unsigned long long)va_arg(ap, unsigned int);
            int len = kformat_uint(buf, val, base, uppercase);
            kput_field(0, buf, len, width, left_align, zero_pad);
            break;
        }
        case 'p': {
            int len = kformat_uint(buf, (uintptr_t)va_arg(ap, void *), 16, 0);
            kput_field("0x", buf, len, width, left_align, zero_pad);
            break;
        }
        case 'c':
            buf[0] = (char)va_arg(ap, int);
            kput_field(0, buf, 1, width, left_align, 0);
            break;
        case 's': {
            const char *str = va_arg(ap, const char *);
            if (!str) str = "(null)";
            kput_field(0, str, (int)strlen(str), width, left_align, 0);
            break;
        }
        case '%':
            terminal_putchar('%');
            break;
        case '\0':
            return;
        default:
            terminal_putchar('%');
            terminal_putchar(*fmt);
            break;
        }

        fmt++;
    }
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    /* 한 번의 kprintf 출력이 다른 프로세스 출력과 섞이지 않게 한다.
     * 패닉 경로에서도 불리므로 재귀 검사가 있는 spinlock 대신 인터럽트 차단만 쓴다 */
    uint32_t flags = irq_save();
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    irq_restore(flags);
}