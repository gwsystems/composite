#include <stdarg.h>
#include <llprint.h>
#include "cos_mc_adapter.h"

#define PRINT_BUF_SZ 512

int
cos_printc(const char *fmt, va_list ap)
{
	size_t  ret;
	char print_buffer[PRINT_BUF_SZ];

	ret = vsnprintf(print_buffer, PRINT_BUF_SZ, fmt, ap);
	cos_llprint(print_buffer, ret);

	return ret;
}

int
cos_printf(const char *fmt,...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = cos_printc(fmt, ap);
	va_end(ap);
	return ret;
}

int
cos_getpeername(int fd, struct sockaddr *restrict addr, socklen_t *restrict len)
{
	/* TODO: get client's address */
	return 0;
}
