#include <cos_component.h>
#include <cos_alloc.h>
#include <print.h>
#include <valloc.h>
#include <stdlib.h> 		/* rand */
#include <cbuf.h>
#include <cbuf_mgr.h>

#include <pgfault.h>

#include <unit_cbuf.h>
#include <unit_cbufp.h>

/* Default args */
#define MAX_CBUFS   200
#define MAX_CBUF_SZ 4096
#define MAX_CBUFP_SZ 4096
#define MAX_CBUFPS 32

//#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif

void
cbuf_tests(void)
{
	cbuf_t cbs[MAX_CBUFS];
	int szs[MAX_CBUFS];
	char *bufs[MAX_CBUFS];
	int i;

	printc("\nUNIT TEST (CBUF)\n");

	for (i = 0 ; i < MAX_CBUFS ; i++) {
		int sz = (rand() % MAX_CBUF_SZ) + 1;
		bufs[i] = cbuf_alloc_ext(sz, &cbs[i], CBUF_TMEM);
		printv("UNIT TEST alloc %d -> %p\n", sz, bufs[i]);
		assert(bufs[i]);
		cbuf_free(cbs[i]);
		printv("UNIT TEST free %p\n", bufs[i]);
	}

	printc("UNIT TEST PASSED: alloc->dealloc\n");
	for (i = 0 ; i < MAX_CBUFS ; i++) {
		int sz = (rand() % MAX_CBUF_SZ) + 1;
		bufs[i] = cbuf_alloc_ext(sz, &cbs[i], CBUF_TMEM);
		printv("UNIT TEST alloc %d -> %p\n", sz, bufs[i]);
		assert(bufs[i]);
	}

	for (i = 0 ; i < MAX_CBUFS ; i++) {
		cbuf_free(cbs[i]);
		printv("UNIT TEST free %p\n", bufs[i]);
	}
	printc("UNIT TEST PASSED: N alloc -> N dealloc\n");

	for (i = 0 ; i < MAX_CBUFS ; i++) {
		int sz = (rand() % MAX_CBUF_SZ) + 1;
		bufs[i] = cbuf_alloc_ext(sz, &cbs[i], CBUF_TMEM);
		printv("UNIT TEST alloc %d -> %p\n", sz, bufs[i]);
		szs[i] = sz;
		assert(bufs[i]);
		bufs[i][0] = '_';
		cbuf_send(cbs[i]);
		unit_cbuf(cbs[i], sz);
		printv("UNIT TEST cbuf2buf %d\n", sz);
	}
	printc("UNIT TEST PASSED: N alloc + cbuf2buf\n");
	
	for (i = 0 ; i < MAX_CBUFS ; i++) {
		bufs[i][0] = '_';
		cbuf_send(cbs[i]);
		unit_cbuf(cbs[i], szs[i]);
		printv("UNIT TEST cbuf2buf %d\n", szs[i]);
	}
	printc("UNIT TEST PASSED: N cached cbuf2buf\n");
	for (i = 0 ; i < MAX_CBUFS ; i++) {
		cbuf_free(cbs[i]);
		printv("UNIT TEST free %p\n", bufs[i]);
	}
	printc("UNIT TEST PASSED: N deallocs\n");
	
	printc("UNIT TEST (CBUF) ALL PASSED\n");
}

void
cbufp_tests()
{
	cbuf_t cbs[MAX_CBUFPS];
	int sz = MAX_CBUFP_SZ;
	char *bufs[MAX_CBUFPS];
	int i;

	printc("spd: %d\n", cos_spd_id());
	printc("\nUNIT TEST (CBUFP)\n");

	for (i = 0 ; i < MAX_CBUFPS ; i++) {
		cbs[i] = unit_cbufp_alloc(sz);
		assert(cbs[i]);
		bufs[i] = cbuf2buf(cbs[i], sz);
		assert(bufs[i]);
		cbuf_send_free(cbs[i]);
		unit_cbufp2buf(cbs[i], sz); /* error path test */
	}
	printc("UNIT TEST PASSED: N alloc + cbufp2buf\n");

	for (i = 0 ; i < MAX_CBUFPS ; i++) {
		int sz = MAX_CBUFP_SZ;
		bufs[i] = cbuf2buf(cbs[i], sz);
		assert(bufs[i]);
		cbuf_free(cbs[i]);
	}
	printc("UNIT TEST PASSED: N cached cbufp2buf\n");

	for (i = 0 ; i < MAX_CBUFPS ; i++) {
		unit_cbufp_deref(cbs[i], sz);
	}
	printc("UNIT TEST PASSED: N deallocs\n");

	for (i = 0 ; i < MAX_CBUFPS ; i++) {
		spdid_t myspd = cos_spd_id();
		int err = 0;
		/* Assume this component only uses 1 pgd, and there is
		 * unallocated memory at the end for MAX_CBUFPS pages */
		bufs[i] = (char*)(round_up_to_pgd_page(cbufp_tests) - (i+1)*PAGE_SIZE);
		cbs[i] = unit_cbufp_alloc(sz);
		assert(cbs[i]);
		err = unit_cbufp_map_at(cbs[i], sz, myspd, (vaddr_t)bufs[i]);
		assert(!err);
		assert(bufs[i][0] == '_');
	}
	printc("UNIT TEST PASSED: N map_at\n");

	for (i = 0 ; i < MAX_CBUFPS ; i++) {
		spdid_t myspd = cos_spd_id();
		int err = 0;
		err = unit_cbufp_unmap_at(cbs[i], sz, myspd, (vaddr_t)bufs[i]);
		assert(!err);
		bufs[i] = cbuf2buf(cbs[i], sz); /* clear send cnt */
		assert(bufs[i]);
		cbuf_free(cbs[i]);
		unit_cbufp_deref(cbs[i], sz);
	}
	printc("UNIT TEST PASSED: N unmap_at\n");


	printc("UNIT TEST (CBUFP) ALL PASSED\n");
}
