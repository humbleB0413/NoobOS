#pragma once

/*
 * String functions originally adapted from Linux 0.01 (Linus Torvalds).
 * Replaced with plain C because the original x86 inline-asm clobber lists
 * conflict with modern GCC's register-constraint rules (-Wall -Wextra).
 */

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

static inline char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

static inline char *strncpy(char *dest, const char *src, int count) {
    char *d = dest;
    while (count-- > 0) {
        if (!(*d++ = *src++)) {
            while (count-- > 0) *d++ = '\0';
            break;
        }
    }
    return dest;
}

static inline char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

static inline char *strncat(char *dest, const char *src, int count) {
    char *d = dest;
    while (*d) d++;
    while (count-- > 0 && *src) *d++ = *src++;
    *d = '\0';
    return dest;
}

static inline int strcmp(const char *cs, const char *ct) {
    while (*cs && *cs == *ct) { cs++; ct++; }
    return (unsigned char)*cs - (unsigned char)*ct;
}

static inline int strncmp(const char *cs, const char *ct, int count) {
    while (count-- > 0) {
        if (*cs != *ct) return (unsigned char)*cs - (unsigned char)*ct;
        if (!*cs) return 0;
        cs++; ct++;
    }
    return 0;
}

static inline char *strchr(const char *s, char c) {
    while (*s && *s != c) s++;
    return *s == c ? (char *)s : (char *)0;
}

static inline char *strrchr(const char *s, char c) {
    const char *last = (char *)0;
    while (*s) { if (*s == c) last = s; s++; }
    return (char *)last;
}

static inline int strspn(const char *cs, const char *ct) {
    const char *s = cs;
    while (*s) {
        const char *p = ct;
        while (*p && *p != *s) p++;
        if (!*p) break;
        s++;
    }
    return s - cs;
}

static inline int strcspn(const char *cs, const char *ct) {
    const char *s = cs;
    while (*s) {
        const char *p = ct;
        while (*p) { if (*p == *s) return s - cs; p++; }
        s++;
    }
    return s - cs;
}

static inline char *strpbrk(const char *cs, const char *ct) {
    while (*cs) {
        const char *p = ct;
        while (*p) { if (*p == *cs) return (char *)cs; p++; }
        cs++;
    }
    return (char *)0;
}

static inline char *strstr(const char *cs, const char *ct) {
    int len = 0;
    const char *p = ct;
    while (*p++) len++;
    if (!len) return (char *)cs;
    while (*cs) {
        if (*cs == *ct) {
            const char *a = cs, *b = ct;
            int n = len;
            while (n-- && *a++ == *b++);
            if (n < 0) return (char *)cs;
        }
        cs++;
    }
    return (char *)0;
}

static inline int strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return p - s;
}

static inline void *memcpy(void *dest, const void *src, int n) {
    char *d = dest;
    const char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

static inline void *memmove(void *dest, const void *src, int n) {
    char *d = dest;
    const char *s = src;
    if (d <= s || d >= s + n) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

static inline int memcmp(const void *cs, const void *ct, int count) {
    const unsigned char *s1 = cs, *s2 = ct;
    while (count--) {
        if (*s1 != *s2) return *s1 - *s2;
        s1++; s2++;
    }
    return 0;
}

static inline void *memchr(const void *cs, char c, int count) {
    const char *s = cs;
    while (count--) { if (*s == c) return (void *)s; s++; }
    return (void *)0;
}

static inline void *memset(void *s, char c, int count) {
    char *d = s;
    while (count--) *d++ = c;
    return s;
}
