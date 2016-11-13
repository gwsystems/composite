#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cobj_format.h>

#ifndef _cFE_util_
#define _cFE_util_

int
prints(char *s);

int __attribute__((format(printf,1,2))) printc(char *fmt, ...);

#define PANIC(a) panic_impl(__func__, a)

void panic_impl(const char* function, char* message);

void __isoc99_sscanf(void);

#endif
