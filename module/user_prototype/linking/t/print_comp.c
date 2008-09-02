#include <cos_component.h>

#include <stdio.h>
#include <string.h>
#define MAX_LEN 512

char foo[MAX_LEN];

/* strings must have 3 variables to print... */
int validate_format(char *s, int len)
{
	int cnt = 0;

	while (*s != '\0' && len) {
		if (*s == '%') cnt++;
		len--;
		s++;
	}
	if (cnt != 3) return -1;
	return 0;
}
//int print_vals(int val1, int val2, int val3, int val4);
int printfmt(short int *s, unsigned int a, unsigned int b, unsigned int c)
{
	char *string = (char*)&s[1];
	short int len;

	if (!COS_IN_ARGREG(s)) {
		snprintf(foo, MAX_LEN, "print argument out of bounds: %x", (unsigned int)s);
		cos_print(foo, 0);
		return -1;
	} 

	len = *s;
	if (validate_format(string, MAX_LEN)) {
		snprintf(foo, MAX_LEN, "incorrect format string for print: %s", string);
	} else {
		snprintf(foo, MAX_LEN, string, a, b, c);
	}
	cos_print(foo, 0);

	return 0;
}

int print_str(char *s, unsigned int len)
{
//	char *ptr;
//	int l = len;

	if (!COS_IN_ARGREG(s) || !COS_IN_ARGREG(s + len)) {
		snprintf(foo, MAX_LEN, "print argument out of bounds: %x", (unsigned int)s);
		cos_print(foo, 0);
		return -1;
	}
	s[len-1] = '\0';

	cos_print(s, len);

	return 0;
}

int main(void)
{
	return 0;
}
