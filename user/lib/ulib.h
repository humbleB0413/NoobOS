#pragma once

/* NoobOS 유저 프로그램용 최소 C 라이브러리 (시스템 콜 래퍼 + 문자열 + printf) */

#include <stdarg.h>
#include <stdint.h>
#include "uapi/syscall_nr.h"

typedef unsigned int size_t;
#define NULL ((void *)0)

__attribute__((noreturn)) void exit(int code);
int write(int fd, const void *buf, size_t len);
int read(int fd, void *buf, size_t len);
int getpid(void);
int sleep_ms(unsigned ms);
int yield(void);
unsigned uptime_ms(void);
int open(const char *path);
int close(int fd);
/* 반환: 1=파일, 2=디렉터리, 0=끝, 음수=오류 */
int readdir(unsigned index, char *buf, size_t len);
int spawn(const char *path, const char *argline);
int wait(int pid);

size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
int atoi(const char *s);

int putchar(int c);
int puts(const char *s);
/* 지원: %d %u %x %s %c %% 와 폭/'-'/'0' */
int printf(const char *fmt, ...);
const char *strerror(int err);
