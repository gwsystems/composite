#include "string.h"
#include "vtxprintf.h"

static char *str_buf;

void *
memcpy(void *dst, const void *src, size_t count)
{
	const u8_t *s = (const u8_t *)src;
	u8_t *      d = (u8_t *)dst;

	for (; count != 0; count--) *d++ = *s++;

	return dst;
}

void *
memset(void *dst, int c, size_t count)
{
	char *p = (char *)dst;

	for (; count != 0; count--) *p++ = c;

	return dst;
}

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
