#ifndef LLPRINT_H
#define LLPRINT_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cos_serial.h>

#define LLPRINT_MAX_LEN 128
#define LLPRINT_SERIAL_MAX_LEN 128

static void
cos_llprint(char *s, int len)
{
	cos_serial_puts(s);
}

static int
prints(char *s)
{
	size_t len = strlen(s);

	cos_print(s, len); /* use syscall to print, so it prints to vga as well */

	return len;
}

static int  __attribute__((format(printf, 1, 2)))
printc(char *fmt, ...)
{
	char    s[LLPRINT_MAX_LEN];
	va_list arg_ptr;
	size_t  ret, len = LLPRINT_MAX_LEN;

	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, len, fmt, arg_ptr);
	va_end(arg_ptr);
	cos_llprint(s, ret);

	return ret;
}

typedef enum {
	PRINT_ERROR = 0, /* print only error messages */
	PRINT_WARN,      /* print errors and warnings */
	PRINT_INFO,      /* print info messages, errors, and warnings */
	PRINT_DEBUG      /* print errors, warnings, info messsages, and debug messages */
} cos_print_level_t;

#define PRINT_LEVEL_COUNT 4

extern cos_print_level_t  cos_print_level;
extern int                cos_print_lvl_str;
extern const char        *cos_print_str[];

/* Prints with current (cpuid, thdid, spdid) */
#define PRINTC(format, ...) printc("(%ld,%u,%lu) " format, cos_cpuid(), cos_thdid(), cos_spd_id(), ## __VA_ARGS__)

/* Prints only if @level is <= cos_print_level */
#define PRINTLOG(level, format, ...)                                                          \
	{                                                                                     \
		if (level <= cos_print_level) {                                               \
			PRINTC("%s" format,                                                   \
			       cos_print_lvl_str ? cos_print_str[level] : "", ##__VA_ARGS__); \
		}                                                                             \
	}

#define PRINT_LOG PRINTLOG

static void
log_bytes(cos_print_level_t level, char *message, char *bytes, size_t bytes_count)
{
	if (level <= cos_print_level) {
		size_t i;

		PRINT_LOG(level, "%s [ ", message);
		for (i = 0; i < bytes_count; i++) {
			printc("%X ", bytes[i]);
		}
		printc("]\n");
	}
}

static void
serial_print(void *d, int len)
{
	call_cap(PRINT_CAP_TEMP, (int)d, len, 1 /* serial print */, 0);
}

#endif /* LLPRINT_H */
