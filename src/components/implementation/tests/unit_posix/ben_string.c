#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFLEN 500000

size_t b_string_strstr(void *dummy)
{
	const char *needle = dummy;
	size_t l = strlen(needle);
	size_t i;
	size_t cnt = 10000;
	size_t cs = 0;
	char *haystack = malloc(l * cnt + 1);
	for (i=0; i<cnt-1; i++) {
		memcpy(haystack + l*i, needle, l);
		haystack[l*i+l-1] ^= 1;
	}
	memcpy(haystack + l*i, needle, l+1);
	for (i=0; i<50; i++) {
		haystack[0]^=1;
		cs += (int)strstr(haystack, needle);
	}
	free(haystack);
	return cs;
}

size_t b_string_memset(void *dummy)
{
	char *buf = malloc(BUFLEN);
	size_t i;
	for (i=0; i<100; i++)
		memset(buf+i, i, BUFLEN-i);
	free(buf);
	return 0;
}

size_t b_string_strchr(void *dummy)
{
	char *buf = malloc(BUFLEN);
	size_t i;
	size_t cs;
	memset(buf, 'a', BUFLEN);
	buf[BUFLEN-1] = 0;
	buf[BUFLEN-2] = 'b';
	for (i=0; i<100; i++) {
		buf[i] = '0'+i%8;
		cs += (int)strchr(buf, 'b');
	}
	free(buf);
	return cs;
}

size_t b_string_strlen(void *dummy)
{
	char *buf = malloc(BUFLEN);
	size_t i;
	size_t cs = 0;

	memset(buf, 'a', BUFLEN-1);
	buf[BUFLEN-1] = 0;
	for (i=0; i<100; i++) {
		buf[i] = '0'+i%8;
		cs += strlen(buf);
	}
	free(buf);
	return cs;
}
