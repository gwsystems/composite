#ifndef LLPRINT_H
#define LLPRINT_H

#include <cos_types.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cos_serial.h>

static void
cos_llprint(char *s, uword_t len)
{
	cos_serial_putb(s, len);
}

int cos_print_str(char *s, uword_t len);

static int
prints(char *s)
{
	return cos_print_str(s, strlen(s));
}

static int  __attribute__((format(printf, 1, 2)))
printc(char *fmt, ...)
{
	char    s[180];
	va_list arg_ptr;
	size_t  ret;

	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, 180, fmt, arg_ptr);
	va_end(arg_ptr);
	ret = cos_print_str(s, (uword_t)ret);

	return ret;
}

/* Prints with current (cpuid, thdid, spdid) */
#define PRINTC(format, ...) printc("(%ld,%lu,%lu) " format, cos_cpuid(), cos_thdid(), cos_compid(), ## __VA_ARGS__)
#define PRINTLOG(level, format, ...) PRINTC(format, ##__VA_ARGS__)

#endif /* LLPRINT_H */
