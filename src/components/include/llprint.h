#ifndef LLPRINT_H
#define LLPRINT_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <cos_component.h>

static void
cos_llprint(char *s, int len)
{
	call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0);
}

static int
prints(char *s)
{
	size_t len = strlen(s);

	cos_llprint(s, len);

	return len;
}

static int  __attribute__((format(printf, 1, 2)))
printc(char *fmt, ...)
{
	char    s[128];
	va_list arg_ptr;
	size_t  ret, len = 128;

	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, len, fmt, arg_ptr);
	va_end(arg_ptr);
	cos_llprint(s, ret);

	return ret;
}

#endif /* LLPRINT_H */
