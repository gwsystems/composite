#ifndef PRINT_H
#define PRINT_H

#include <cos_component.h>

static inline void strcpn(char *to, char *from, int n)
{
	while (*from != '\0' && n--) {
		*to = *from;
		from++;
		to++;
	}
	/* ensure null termination */
	if (0 == n) {
		*from = '\0';
	}
}

static inline int strlen(char *s) 
{
	char *t = s;
	while (*t != '\0') t++;

	return t-s;
}

#define ARG_STRLEN 1024
extern int printstr(short int *s, int a, int b, int c);
static inline int print(char *str, int a, int b, int c)
{
	char *d;
	short int len, *s;

	len = strlen(str) + 1; /* + 1 for '\0' */
	s = cos_argreg_alloc(len + sizeof(short int));
	if (!s) return -1;

	*s = len;
	d = (char*)&s[1];
	cos_memcpy(d, str, len);
	printstr(s, a, b, c);
	if (cos_argreg_free(s)) return -1;

	return 0;
}

#endif
