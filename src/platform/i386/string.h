#ifndef STRING_H
#define STRING_H

#include "vtxprintf.h"
#include "shared/cos_types.h"
#include "chal/defs.h"

typedef unsigned long int size_t;

size_t strnlen(const char *str, size_t max);
int vsprintf(char *buf, const char *fmt, va_list args);

#endif /* STRING_H */
