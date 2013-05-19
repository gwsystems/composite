#ifndef _STRING_H_
#define _STRING_H_

#include "vtxprintf.h"
#include "types.h"

void *memcpy(void *dst, const void *src, size_t count);
void *memset(void *dst, int c, size_t count);
void *wmemset(void *dst, int c, size_t count);
size_t strlen(const char *str);
size_t strnlen(const char *str, size_t max);
int sprintf(char *buf, const char *fmt, ...);
int vsprintf(char *buf, const char *fmt, va_list args);
int strcmp(const char *s1, const char *s2);
#define streq(a,b) (strcmp((a),(b)) == 0)

#endif
