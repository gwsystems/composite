#include <cos_component.h>
#include <cos_config.h>

#include <stdio.h>
#include <string.h>
#define MAX_LEN 512

#include <printc.h>
#define assert(x) do { if (!(x)) *((int*)0) = 0; } while(0)
#define REDEFINE_ASSERT
#include <cringbuf.h>

struct cringbuf sharedbuf;

static int
print_init(void)
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
		if (cos_trans_cntl(COS_TRANS_MAP, COS_TRANS_SERVICE_PRINT, (unsigned long)addr, i)) return -4;
	}
	cringbuf_init(&sharedbuf, start, sz);

	return 0;
}

int
print_str(char *s, unsigned int len)
{
//	if (!COS_IN_ARGREG(s) || !COS_IN_ARGREG(s + len)) {
	if (1) {
		static char foo[MAX_LEN];
		snprintf(foo, MAX_LEN, "print argument out of bounds: %x", (unsigned int)s);
		cos_print(foo, 0);
		return -1;
	}
	s[len+1] = '\0';

#ifdef COS_PRINT_SHELL
	assert(!print_init());

	if (sharedbuf.b) {
		int amnt;

		amnt = cringbuf_produce(&sharedbuf, s, len);
		assert(amnt >= 0);
		cos_trans_cntl(COS_TRANS_TRIGGER, COS_TRANS_SERVICE_PRINT, 0, 0);
	}
#endif

#ifdef COS_PRINT_DMESG
	cos_print(buf_ptr, len);
#endif

	return 0;
}

int
print_char(int len, int a, int b, int c)
{
	assert(0);
	return 0;
}

void print_null(void)
{
	return;
}
