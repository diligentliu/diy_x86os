#ifndef OS_KLIB_H
#define OS_KLIB_H

#include <stdarg.h>
#include "comm/types.h"

static inline uint32_t down2(uint32_t n, uint32_t align) {
	return n & ~(align - 1);
}

static inline uint32_t up2(uint32_t n, uint32_t align) {
	return (n + align - 1) & ~(align - 1);
}

void kernel_strcpy(char *dest, const char *src);
void kernel_strncpy(char *dest, const char *src, int size);
int kernel_strncmp(const char *s1, const char *s2, int size);
int kernel_strlen(const char *str);
void kernel_memcpy(void *dest, void *src, int size);
void kernel_memset(void *dest, uint8_t v, int size);
int kernel_memcmp(void *d1, void *d2, int size);
void kernel_vsprintf(char *buffer, const char *fmt, va_list args);
void kernel_itoa(char *buf, int num, int base);
void kernel_sprintf(char *buffer, const char *fmt, ...);

#ifndef RELEASE
#define ASSERT(condition)    \
    if (!(condition)) panic(__FILE__, __LINE__, __func__, #condition)
void panic(const char *file, int line, const char *func, const char *cond);
#else
#define ASSERT(condition)    ((void)0)
#endif

char *get_file_name(const char *path);
int strings_count(char **start);

#endif