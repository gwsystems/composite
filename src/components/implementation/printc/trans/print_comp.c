#include <cos_component.h>

#include <stdio.h>
#include <string.h>
#define MAX_LEN 512

#include <printc.h>

static char foo[MAX_LEN];
int print_str(char *s, unsigned int len)
{
//	char *ptr;
//	int l = len;

	if (!COS_IN_ARGREG(s) || !COS_IN_ARGREG(s + len)) {
		snprintf(foo, MAX_LEN, "print argument out of bounds: %x", (unsigned int)s);
		cos_print(foo, 0);
		return -1;
	}
	s[len+1] = '\0';

	cos_print(s, len);

	cos_trans_cntl(COS_TRANS_TRIGGER, 0);

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
