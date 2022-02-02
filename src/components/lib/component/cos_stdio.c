#include<llprint.h>
#include<cos_stdio.h>

int
cos_printc(char *fmt, va_list ap)
{
	char    s[128];
	size_t  ret, len = 128;

	ret = vsnprintf(s, len, fmt, ap);
	cos_llprint(s, ret);

	return ret;
}

int
cos_printf(char *fmt,...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = cos_printc(fmt, ap);
	va_end(ap);
	return ret;
}