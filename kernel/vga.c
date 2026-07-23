#include "linux/vga.h"

#include "string.h"

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer = (uint16_t*)VGA_MEMORY;

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
}

void terminal_setcolor(uint8_t color) { terminal_color = color; }

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar(char c) {
  switch (c) {
    case '\n':
      terminal_column = 0;
      terminal_row += 1;
      break;
    case '\t':
      terminal_column += 4;
      break;
    default:
      terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
      if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) terminal_row = 0;
      }
  }
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