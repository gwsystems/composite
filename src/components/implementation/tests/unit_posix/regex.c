#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <locale.h>

size_t
b_regex_compile(void *s)
{
	regex_t re;
	size_t i;

	setlocale(LC_CTYPE, "");
	for (i=0; i<1000; i++) {
		regcomp(&re, s, REG_EXTENDED);
		regfree(&re);
	}
}

char buf[260000];

size_t
b_regex_search(void *s)
{
	regex_t re;
	size_t i;

	setlocale(LC_CTYPE, "");
	memset(buf, 'a', sizeof(buf)-2);
	buf[sizeof buf - 2] = 'b';
	buf[sizeof buf - 1] = 0;
	regcomp(&re, s, REG_EXTENDED);
	regexec(&re, buf, 0, 0, 0);
	regfree(&re);
}
