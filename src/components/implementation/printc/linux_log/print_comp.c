#include <cos_component.h>

#include <stdio.h>
#include <string.h>
#define MAX_LEN 512

#include <printc.h>

static char foo[MAX_LEN];

volatile int l = 0;
int print_str(char *s, unsigned int len)
{
//	char *ptr;
//	int l = len;
	while (l != 0) ;
	l = 1;
	if (!COS_IN_ARGREG(s) || !COS_IN_ARGREG(s + len)) {
		snprintf(foo, MAX_LEN, "print argument out of bounds: %x", (unsigned int)s);
		cos_print(foo, 0);
		return -1;
	}
	s[len+1] = '\0';

	cos_print(s, len);
	l = 0;
	return 0;
}

void print_null(void)
{
	return;
}

int main(void)
{
	return 0;
}
