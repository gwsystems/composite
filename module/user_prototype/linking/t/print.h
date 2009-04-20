#ifndef PRINT_H
#define PRINT_H

#include <cos_component.h>

#define ARG_STRLEN 1024
extern int printfmt(short int *s, int a, int b, int c);
extern int print_str(char *str, int len);

#ifndef COS_FMT_PRINT
static inline int cos_strlen(char *s) 
{
	char *t = s;
	while (*t != '\0') t++;

	return t-s;
}
#else 
#define cos_strlen strlen
#endif

#ifdef COS_FMT_PRINT
#include <stdio.h>
#include <string.h>
static inline int printc(char *fmt, ...)
{
	char *s;
	va_list arg_ptr;
	int ret, len;

	//len = strlen(fmt)+1;
	len = ARG_STRLEN; //(len > ARG_STRLEN) ? COS_FMT_PRINT : len;
	s = cos_argreg_alloc(len);
	if (!s) return 0;

	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, len, fmt, arg_ptr);
	va_end(arg_ptr);
	print_str(s, ret);
	if (cos_argreg_free(s)) return -1;

	return ret;
}
#define cos_memcpy memcpy
#endif

static inline int print(char *str, int a, int b, int c)
{
	char *d;
	short int len, *s;

	len = cos_strlen(str) + 1; /* + 1 for '\0' */
	s = cos_argreg_alloc(len + sizeof(short int));
	if (!s) return -1;

	*s = len;
	d = (char*)&s[1];
	cos_memcpy(d, str, len);
	printfmt(s, a, b, c);
	if (cos_argreg_free(s)) return -1;

	return 0;
}

static inline int prints(char *str)
{
	unsigned int len;
	char *s;

	len = cos_strlen(str);
	s = cos_argreg_alloc(len);
	if (!s) return -1;

	cos_memcpy(s, str, len+1);
	print_str(s, len);
	if (cos_argreg_free(s)) return -1;

	return 0;
}

#endif
