#include "string.h"
#include "vtxprintf.h"

static char *str_buf;

size_t
strnlen(const char *str, size_t max)
{
	size_t ret;

	for (ret = 0; *str != '\0' && ret < max; str++) ret++;

	return ret;
}

static void
str_tx_byte(unsigned char byte)
{
	*str_buf = byte;
	str_buf++;
}

int
vsprintf(char *buf, const char *fmt, va_list args)
{
	int i;

	str_buf  = buf;
	i        = vtxprintf(str_tx_byte, fmt, args);
	*str_buf = '\0';

	return i;
}

int
strncmp(const char *s1, const char *s2, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		if (s1[i] < s2[i]) return -1;
		if (s1[i] > s2[i]) return 1;
	}
	return 0;
}

void *
memset(void *a, int c, size_t n)
{
	size_t off;
	char  *c_arr = a;

	for (off = 0; off < n; off++) {
		c_arr[off] = (char)c;
	}

	return a;
}
