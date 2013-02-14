#include <cos_component.h>
#include <stdio.h>
#include <string.h>
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

/* the following multicore implementation haven't been tested yet. */

/* static inline int send_str(char *s, unsigned int len) */
/* { */
/* 	int param[PARAMS_PER_INV], pending; */
/* 	unsigned int i = 0, j; */
/* 	char *p;  */

/* 	p = (char *)param;  */

/* 	for (i = 0; i <= len; i += CHAR_PER_INV) { */
/* 		for (j = 0; j < CHAR_PER_INV; j++) { */
/* 			if (s[i + j] == '\0') {  */
/* 				p[j] = '\0';  */
/*                                 /\* if we reach the end of the string, */
/* 				 * then pedding the rest of the */
/* 				 * parameters with \0 *\/ */
/* 				while (++j < CHAR_PER_INV) { p[j] = '\0'; } */
/* 				break;  */
/* 			} else { */
/* 				p[j] = s[i + j]; */
/* 			} */
/* 		} */
/* 		pending = print_str(param[0], param[1], param[2], param[3]); */
/* 		if(p[j-1] =='\0' && pending) {cos_print("BUG1", 4); while (1);} */
/* 		if(p[j-1] != '\0' && !pending) {cos_print("BUG2", 4); while (1);} */
/* 	} */
/* 	return 0; */
/* } */

/* #include <cos_debug.h> */
/* int __attribute__((format(printf,1,2))) */
/* printc(char *fmt, ...) */
/* { */
/* 	char s[MAX_LEN]; */
/* 	va_list arg_ptr; */
/* 	unsigned int ret; */
/* 	int len = MAX_LEN; */

/* 	/\* Stable approach below. *\/ */

/* #if (NUM_CPU > 1) */
/* 	va_start(arg_ptr, fmt); */
/* 	ret = vsnprintf(s, len, fmt, arg_ptr); */
/* 	va_end(arg_ptr); */
/* 	cos_print(s, ret); */
	
/* 	return 0; */
/* #endif */
/* 	va_start(arg_ptr, fmt); */
/* 	ret = vsnprintf(s, ARG_STRLEN-1, fmt, arg_ptr); */
/* 	va_end(arg_ptr); */

/* 	if (ret == 0) goto done; */

/* 	send_str(s, ret); */
	
/* done: */
/* 	return ret; */
/* } */

/* int prints(char *str) */
/* { */
/* 	unsigned int len; */

/* 	len = cos_strlen(str); */

/* 	if (len == 0) goto done; */

/* 	send_str(str, len); */
/* done: */
/* 	return 0; */
/* } */
