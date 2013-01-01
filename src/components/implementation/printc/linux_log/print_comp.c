#include <cos_component.h>

#include <stdio.h>
#include <string.h>
#define MAX_LEN 512

#include <printc.h>

static char foo[MAX_LEN];
int
print_str(char *s, unsigned int len)
{
	char n = *((char*)NULL);
	return (int)n;
}

union pstr {
	int  i[4];
	char c[16];
};

int
print_char(int len, int a, int b, int c)
{
	int maxlen = sizeof(int) * 3 + 1;
	union pstr str;

	if (len > maxlen-1) return -1;
	str.i[0] = a;
	str.i[1] = b;
	str.i[2] = c;
	str.c[len] = '\0';

	memcpy(foo, str.c, len+1);
	cos_print(foo, len);

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
