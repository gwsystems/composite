#include <cos_component.h>
#include <cos_config.h>

#include <stdio.h>
#include <string.h>

#include <print.h>
#include <sched.h>

#include <cringbuf.h>

struct cringbuf sharedbuf;

int cos_init(void)
{
	static int first = 1;
	char *addr, *start;
	unsigned long i, sz;

	printc("ctol meas initing...\n");
	if (first) {
		union sched_param sp;
		first = 0;
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 20;
		if (sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();
		return 0;
	}
	/* printc("zz\n"); */

	return 0;

	printc("ctol meas initing1...\n");
	sz = cos_trans_cntl(COS_TRANS_MAP_SZ, 0, 0, 0);
	printc("sz %lu\n", sz);
	if (sz > (8*1024*1024)) return -1;
	printc("ctol meas initing2...\n");
	addr = start = cos_get_vas_page();
	if (!start) return -2;
	printc("ctol meas initing3...\n");
	for (i = PAGE_SIZE ; i < sz ; i += PAGE_SIZE) {
		char *next_addr = cos_get_vas_page();
		if ((((unsigned long)next_addr) - (unsigned long)addr) != PAGE_SIZE) return -3;
		addr = next_addr;
	}
	printc("ctol meas initing4...\n");
	for (i = 0, addr = start ; i < sz ; i += PAGE_SIZE, addr += PAGE_SIZE) {
		if (cos_trans_cntl(COS_TRANS_MAP, COS_TRANS_SERVICE_PRINT, (unsigned long)addr, i)) return -4;
	}
	printc("ctol meas initing5...\n");
	cringbuf_init(&sharedbuf, start, sz);

	printc("ctol meas init done. Going to send data.\n");

	assert(sharedbuf.b);
#define ITER (100*1024)
	unsigned long long t;
	for (i = 0; i < ITER; i++) {
		int amnt;
		rdtscll(t);
		amnt = cringbuf_produce(&sharedbuf, (char *)&t, 8);
		assert(amnt >= 0);
		cos_trans_cntl(COS_TRANS_TRIGGER, COS_TRANS_SERVICE_PRINT, 0, 0);
	}


	return 0;
}
