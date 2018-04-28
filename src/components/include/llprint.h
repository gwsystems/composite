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

/* Prints with current spdid and the thdid */
#define PRINTC(format, ...) printc("%lu,%u=> " format, cos_spd_id(), cos_thdid(), ## __VA_ARGS__)

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


#endif /* LLPRINT_H */
