#ifndef RUMPCALLS_H
#define RUMPCALLS_H

#include <stddef.h>
#include <stdio.h>

typedef __builtin_va_list va_list;
#define va_start(ap, last)      __builtin_va_start((ap), (last))
#define va_arg                  __builtin_va_arg
#define va_end(ap)              __builtin_va_end(ap)

struct cos_rumpcalls
{
	int (*rump_vsnprintf)(char* str, size_t size, const char *format, va_list arg_ptr);
	void (*rump_cos_print)(char s[], int ret);
};

/* Mapping the functions from rumpkernel to composite */
void cos2rump_setup(void);

#endif /* RUMPCALLS_H */
