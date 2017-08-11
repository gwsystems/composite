#ifndef PRINT_H
#define PRINT_H

#include <string.h>
#include <cos_debug.h>
extern int __attribute__((format(printf, 1, 2))) printc(char *fmt, ...);
extern int prints(char *str);

#endif
