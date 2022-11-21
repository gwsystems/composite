#ifndef LLPRINT_H
#define LLPRINT_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cos_serial.h>

static void
cos_llprint(char *s, int len)
{
	cos_serial_putb(s, len);
}

int cos_print_str(char *s, int len);

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
	ret = cos_print_str(s, ret);

	return ret;
}

typedef enum {
	PRINT_ERROR = 0, /* print only error messages */
	PRINT_WARN,	 /* print errors and warnings */
	PRINT_DEBUG,	 /* print errors, warnings and debug messages */
	PRINT_LEVEL_MAX
} cos_print_level_t;

extern int         cos_print_level;
extern int         cos_print_lvl_str;
extern const char *cos_print_lvl[];

/* Prints with current (cpuid, thdid, spdid) */
#define PRINTC(format, ...) printc("(%ld,%lu,%lu) " format, cos_cpuid(), cos_thdid(), cos_compid(), ## __VA_ARGS__)
/* Prints only if @level is <= cos_print_level */
#define PRINTLOG(level, format, ...)                                                          \
	{                                                                                     \
		if (level <= cos_print_level) {                                               \
			PRINTC("%s" format,                                                   \
			       cos_print_lvl_str ? cos_print_lvl[level] : "", ##__VA_ARGS__); \
		}                                                                             \
	}

#endif /* LLPRINT_H */
