#include <cos_component.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <printc.h>

#define ARG_STRLEN 512

#define COS_FMT_PRINT
#ifndef COS_FMT_PRINT
int
cos_strlen(char *s) 
{
	char *t = s;
	while (*t != '\0') t++;

	return t-s;
}
#else 
#define cos_strlen strlen
#endif
#define cos_memcpy memcpy

int
prints(char *str)
{
	int left;
	char *off;
	const int maxsend = sizeof(int) * 3;

	if (!str) return -1;
	for (left = cos_strlen(str), off = str ; 
	     left > 0 ; 
	     left -= maxsend, off += maxsend) {
		int *args;
		int l = left < maxsend ? left : maxsend;
		char tmp[maxsend];

		cos_memcpy(tmp, off, l);
		args = (int*)tmp;
		print_char(l, args[0], args[1], args[2]);
	} 
	return 0;
}

#include <cos_debug.h>
int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	char s[ARG_STRLEN];
	va_list arg_ptr;
	int ret;

	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, ARG_STRLEN-1, fmt, arg_ptr);
	va_end(arg_ptr);
	s[ARG_STRLEN-1] = '\0';
#if (NUM_CPU > 1)
	cos_print(s, ret);
#else
	prints(s);
#endif

	return ret;
}
