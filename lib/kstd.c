#include "kstd.h"
#include "string.h"
#include "../mm/vmm.h"

static int digit_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 99;
}

unsigned long strtoul(const char *s, const char **end, int base) {
  unsigned long value = 0;

  while (isspace(*s)) s++;
  if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    s += 2;
    base = 16;
  }
  if (base == 0) base = 10;

  for (int d; (d = digit_value(*s)) < base; s++) {
    value = value * base + d;
  }
  if (end) *end = s;
  return value;
}

int atoi(const char *s) {
  while (isspace(*s)) s++;
  int negative = (*s == '-');
  if (*s == '-' || *s == '+') s++;
  int value = (int)strtoul(s, 0, 10);
  return negative ? -value : value;
}

char *kstrdup(const char *s) {
  int len = strlen(s);
  char *copy = kmalloc(len + 1);
  if (copy) memcpy(copy, s, len + 1);
  return copy;
}
