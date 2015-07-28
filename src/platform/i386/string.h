#ifndef STRING_H
#define STRING_H

#include "vtxprintf.h"
#include "shared/cos_types.h"

typedef unsigned long int size_t;

void *memset(void *dst, int c, size_t count);
size_t strnlen(const char *str, size_t max);
int vsprintf(char *buf, const char *fmt, va_list args);
int strncmp(const char *s1, const char *s2, size_t n);

#endif /* STRING_H */
