#include "linux/vga.h"

#include "string.h"
#include "port_io.h"

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

static void kprint_int(long n)
{
    char buf[32];
    int  i = 0;

    if (n < 0) {
        terminal_putchar('-');
        n = -n;
    }
    if (n == 0) {
        terminal_putchar('0');
        return;
    }
    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    while (i > 0)
        terminal_putchar(buf[--i]);
}

static void kprint_uint(unsigned long n, int base, int uppercase)
{
    const char *digits = uppercase ? "0123456789ABCDEF"
                                   : "0123456789abcdef";
    char buf[64];
    int  i = 0;

    if (n == 0) {
        terminal_putchar('0');
        return;
    }
    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }
    while (i > 0)
        terminal_putchar(buf[--i]);
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            terminal_putchar(*fmt++);
            continue;
        }

        fmt++; // skip '%'

        // length modifier
        int is_long      = 0;
        int is_long_long = 0;
        if (*fmt == 'l') {
            fmt++;
            if (*fmt == 'l') { fmt++; is_long_long = 1; }
            else              {        is_long      = 1; }
        }

        switch (*fmt) {
        case 'd': case 'i': {
            long val = is_long_long ? (long)va_arg(ap, long long)
                     : is_long      ? va_arg(ap, long)
                     :                va_arg(ap, int);
            kprint_int(val);
            break;
        }
        case 'u': {
            unsigned long val = is_long_long ? (unsigned long)va_arg(ap, unsigned long long)
                              : is_long      ? va_arg(ap, unsigned long)
                              :                va_arg(ap, unsigned int);
            kprint_uint(val, 10, 0);
            break;
        }
        case 'x': {
            unsigned long val = is_long_long ? (unsigned long)va_arg(ap, unsigned long long)
                              : is_long      ? va_arg(ap, unsigned long)
                              :                va_arg(ap, unsigned int);
            kprint_uint(val, 16, 0);
            break;
        }
        case 'X': {
            unsigned long val = is_long_long ? (unsigned long)va_arg(ap, unsigned long long)
                              : is_long      ? va_arg(ap, unsigned long)
                              :                va_arg(ap, unsigned int);
            kprint_uint(val, 16, 1);
            break;
        }
        case 'o': {
            unsigned long val = is_long_long ? (unsigned long)va_arg(ap, unsigned long long)
                              : is_long      ? va_arg(ap, unsigned long)
                              :                va_arg(ap, unsigned int);
            kprint_uint(val, 8, 0);
            break;
        }
        case 'b': {                          // 커널 디버깅용 binary (비표준이지만 유용)
            unsigned long val = is_long_long ? (unsigned long)va_arg(ap, unsigned long long)
                              : is_long      ? va_arg(ap, unsigned long)
                              :                va_arg(ap, unsigned int);
            kprint_uint(val, 2, 0);
            break;
        }
        case 'c':
            terminal_putchar((char)va_arg(ap, int));
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            terminal_writestring(s ? s : "(null)");
            break;
        }
        case 'p':
            terminal_writestring("0x");
            kprint_uint((uintptr_t)va_arg(ap, void *), 16, 0);
            break;
        case '%':
            terminal_putchar('%');
            break;
        default:
            terminal_putchar('%');
            terminal_putchar(*fmt);
            break;
        }

        fmt++;
    }

    va_end(ap);
}