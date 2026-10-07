#pragma once

#include <stdint.h>

static inline int isdigit(int c) { return c >= '0' && c <= '9'; }
static inline int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static inline int isprint(int c) { return c >= 0x20 && c < 0x7F; }

/* base 0 이면 "0x" 접두어로 16진수를, 아니면 10진수를 추정한다. 숫자가 아닌 첫 문자에서 멈추고 *end 에 위치를 남긴다 */
unsigned long strtoul(const char *s, const char **end, int base);
int atoi(const char *s);
/* kmalloc 으로 복사본을 만든다 — kfree 로 해제 */
char *kstrdup(const char *s);

#ifdef DEBUG
int lib_selftest(void);
#endif
