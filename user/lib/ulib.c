#include "ulib.h"

static inline int syscall3(int nr, uint32_t a, uint32_t b, uint32_t c) {
  int ret;
  __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(a), "c"(b), "d"(c) : "memory");
  return ret;
}

void exit(int code) {
  syscall3(SYS_EXIT, (uint32_t)code, 0, 0);
  for (;;);
}
int write(int fd, const void *buf, size_t len) { return syscall3(SYS_WRITE, fd, (uint32_t)buf, len); }
int read(int fd, void *buf, size_t len) { return syscall3(SYS_READ, fd, (uint32_t)buf, len); }
int getpid(void) { return syscall3(SYS_GETPID, 0, 0, 0); }
int sleep_ms(unsigned ms) { return syscall3(SYS_SLEEP, ms, 0, 0); }
int yield(void) { return syscall3(SYS_YIELD, 0, 0, 0); }
unsigned uptime_ms(void) { return (unsigned)syscall3(SYS_UPTIME, 0, 0, 0); }
int open(const char *path) { return syscall3(SYS_OPEN, (uint32_t)path, 0, 0); }
int close(int fd) { return syscall3(SYS_CLOSE, fd, 0, 0); }
int readdir(unsigned index, char *buf, size_t len) { return syscall3(SYS_READDIR, index, (uint32_t)buf, len); }
int spawn(const char *path, const char *argline) { return syscall3(SYS_SPAWN, (uint32_t)path, (uint32_t)argline, 0); }
int wait(int pid) { return syscall3(SYS_WAIT, pid, 0, 0); }

size_t strlen(const char *s) {
  size_t n = 0;
  while (s[n]) n++;
  return n;
}

int strcmp(const char *a, const char *b) {
  while (*a && *a == *b) { a++; b++; }
  return (unsigned char)*a - (unsigned char)*b;
}

void *memset(void *dst, int c, size_t n) {
  unsigned char *d = dst;
  while (n--) *d++ = (unsigned char)c;
  return dst;
}

void *memcpy(void *dst, const void *src, size_t n) {
  unsigned char *d = dst;
  const unsigned char *s = src;
  while (n--) *d++ = *s++;
  return dst;
}

int atoi(const char *s) {
  int sign = 1, v = 0;
  if (*s == '-') { sign = -1; s++; }
  while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
  return sign * v;
}

/* printf 는 작은 버퍼에 모았다가 한 번의 write 로 내보낸다 — 시스템 콜 수를 줄이고 다른 프로세스 출력과 덜 섞이게 */
static char out_buf[256];
static size_t out_len;

static void out_flush(void) {
  if (out_len) write(1, out_buf, out_len);
  out_len = 0;
}

static void out_char(char c) {
  if (out_len == sizeof(out_buf)) out_flush();
  out_buf[out_len++] = c;
}

int putchar(int c) {
  char ch = (char)c;
  return write(1, &ch, 1);
}

int puts(const char *s) {
  write(1, s, strlen(s));
  return putchar('\n');
}

static void out_field(const char *s, int len, int width, int left, char pad) {
  int fill = width - len;
  if (!left) while (fill-- > 0) out_char(pad);
  for (int i = 0; i < len; i++) out_char(s[i]);
  if (left) while (fill-- > 0) out_char(' ');
}

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char num[16];
  for (; *fmt; fmt++) {
    if (*fmt != '%') { out_char(*fmt); continue; }
    fmt++;
    int left = 0, width = 0;
    char pad = ' ';
    if (*fmt == '-') { left = 1; fmt++; }
    if (*fmt == '0') { pad = '0'; fmt++; }
    while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');

    switch (*fmt) {
      case 'd': case 'u': case 'x': {
        unsigned v = va_arg(ap, unsigned);
        int neg = (*fmt == 'd' && (int)v < 0);
        unsigned base = (*fmt == 'x') ? 16 : 10;
        if (neg) v = -v;
        int n = 0;
        do { num[n++] = "0123456789abcdef"[v % base]; v /= base; } while (v);
        if (neg && pad == '0') { out_char('-'); width--; }   /* -0042: 부호가 0 채움 앞에 */
        else if (neg) num[n++] = '-';
        char rev[16];
        for (int i = 0; i < n; i++) rev[i] = num[n - 1 - i];
        out_field(rev, n, width, left, pad);
        break;
      }
      case 's': {
        const char *s = va_arg(ap, const char *);
        if (!s) s = "(null)";
        out_field(s, (int)strlen(s), width, left, ' ');
        break;
      }
      case 'c': {
        char c = (char)va_arg(ap, int);
        out_field(&c, 1, width, left, ' ');
        break;
      }
      case '%': out_char('%'); break;
      case '\0': fmt--; break;
      default: out_char('%'); out_char(*fmt); break;
    }
  }
  va_end(ap);
  out_flush();
  return 0;
}

const char *strerror(int err) {
  switch (err) {
    case E_NOENT: return "no such file";
    case E_FAULT: return "bad address";
    case E_BADF: return "bad file descriptor";
    case E_INVAL: return "invalid argument";
    case E_MFILE: return "too many open files";
    case E_NOSYS: return "no such system call";
    case -12: return "out of memory";
    default: return "error";
  }
}
