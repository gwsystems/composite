#include <cos_component.h>

#include <stdio.h>
#include <string.h>
#define MAX_LEN 512

#include <printc.h>
#define assert(x) do { int y; if (!(x)) y = *(int*)NULL; } while(0)
#include <cringbuf.h>

struct cringbuf sharedbuf;
static int print_init(void)
{
	static int first = 1;
	char *addr, *start;
	unsigned long i, sz;

	if (!first) return 0;
	first = 0;

	sz = cos_trans_cntl(COS_TRANS_MAP_SZ, 0, 0, 0);
	if (sz > (8*1024*1024)) return -1;

	addr = start = cos_get_vas_page();
	if (!start) return -2;
	for (i = PAGE_SIZE ; i < sz ; i += PAGE_SIZE) {
		char *next_addr = cos_get_vas_page();
		if ((((unsigned long)next_addr) - (unsigned long)addr) != PAGE_SIZE) return -3;
		addr = next_addr;
	}

	for (i = 0, addr = start ; i < sz ; i += PAGE_SIZE, addr += PAGE_SIZE) {
		if (cos_trans_cntl(COS_TRANS_MAP, 0, (unsigned long)addr, i)) return -4;
	}
	sharedbuf.b = (struct __cringbuf *)start;

	return 0;
}

static char foo[MAX_LEN];
int print_str(char *s, unsigned int len)
{
	int r;

	if (!COS_IN_ARGREG(s) || !COS_IN_ARGREG(s + len)) {
		snprintf(foo, MAX_LEN, "print argument out of bounds: %x", (unsigned int)s);
		cos_print(foo, 0);
		return -1;
	}
	s[len+1] = '\0';
	if ((r = print_init())) {
		snprintf(foo, MAX_LEN, "<<WTF: %d>>\n", r);
		cos_print(foo, 13);
	}

	cos_print(s, len);
	if (sharedbuf.b) {
		memcpy(sharedbuf.b->buffer, s, len);
		sharedbuf.b->head = 0;
		sharedbuf.b->tail = len-1;
	}
	cos_trans_cntl(COS_TRANS_TRIGGER, 0, 0, 0);

	return 0;
}

void print_null(void)
{
	return;
}
