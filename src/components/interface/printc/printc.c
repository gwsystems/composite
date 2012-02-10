#include <cos_component.h>
#include <stdio.h>
#include <string.h>
#include <printc.h>

#define ARG_STRLEN 1024 //2048

#define COS_FMT_PRINT
#ifndef COS_FMT_PRINT
int cos_strlen(char *s) 
{
	char *t = s;
	while (*t != '\0') t++;

	return t-s;
}
#else 
#define cos_strlen strlen
#endif
#define cos_memcpy memcpy

int prints(char *str)
{
	unsigned int len;
	char *s;

	len = cos_strlen(str);
	s = cos_argreg_alloc(len);
	if (!s) return -1;

	cos_memcpy(s, str, len+1);
	print_str(s, len);
	cos_argreg_free(s);

	return 0;
}

#include <cos_debug.h>
int __attribute__((format(printf,1,2))) printc(char *fmt, ...)
{
	char *s;
	va_list arg_ptr;
	int ret, len;

	//len = strlen(fmt)+1;
	len = ARG_STRLEN; //(len > ARG_STRLEN) ? COS_FMT_PRINT : len;
	s = cos_argreg_alloc(len);
	if (!s) BUG();

	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, len, fmt, arg_ptr);
	va_end(arg_ptr);
	print_str(s, ret);
	cos_argreg_free(s);

	return ret;
}
