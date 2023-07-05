#pragma once

#include <vtxprintf.h>

typedef unsigned long int size_t;

size_t strnlen(const char *str, size_t max);
int    vsprintf(char *buf, const char *fmt, va_list args);
int    strncmp(const char *s1, const char *s2, size_t n);
void  *memset(void *a, int c, size_t n);
void  *memcpy(void *d, void *s, size_t n);
