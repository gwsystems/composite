#include <cos_component.h>
#include <cos_alloc.h>
#include <print.h>
#include <stdlib.h> 		/* rand */
#include <cbuf.h>
#include <cbufp.h>
#include <unit_cbuf.h>
#include <unit_cbufp.h>

//#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif

#define MAX_CBUFS   200
#define MAX_CBUF_SZ 4096
#define MAX_CBUFP_SZ 4096
#define MAX_CBUFPS 32

static void
cbuf_tests(void)
{
	cbuf_t cbs[MAX_CBUFS];
	int szs[MAX_CBUFS];
	char *bufs[MAX_CBUFS];
	int i;

	printc("\nUNIT TEST (CBUF)\n");

	for (i = 0 ; i < MAX_CBUFS ; i++) {
		int sz = (rand() % MAX_CBUF_SZ) + 1;
		bufs[i] = cbuf_alloc(sz, &cbs[i]);
		printv("UNIT TEST alloc %d -> %p\n", sz, bufs[i]);
		assert(bufs[i]);
		cbuf_free(cbs[i]);
		printv("UNIT TEST free %p\n", bufs[i]);
	}

	printc("UNIT TEST PASSED: alloc->dealloc\n");
	for (i = 0 ; i < MAX_CBUFS ; i++) {
		int sz = (rand() % MAX_CBUF_SZ) + 1;
		bufs[i] = cbuf_alloc(sz, &cbs[i]);
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
		bufs[i] = cbuf_alloc(sz, &cbs[i]);
		printv("UNIT TEST alloc %d -> %p\n", sz, bufs[i]);
		szs[i] = sz;
		assert(bufs[i]);
		bufs[i][0] = '_';
		unit_cbuf(cbs[i], sz);
		assert(bufs[i][0] == '*');
		printv("UNIT TEST cbuf2buf %d\n", sz);
	}
	printc("UNIT TEST PASSED: N alloc + cbuf2buf\n");
	
	for (i = 0 ; i < MAX_CBUFS ; i++) {
		bufs[i][0] = '_';
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

static void
cbufp_tests()
{
	cbufp_t cbs[MAX_CBUFPS];
	int szs[MAX_CBUFPS];
	char *bufs[MAX_CBUFPS];
	int i;
	struct cbuf_alloc_desc *d = &cbufp_alloc_freelists[0];
	assert(EMPTY_LIST(d, next, prev));

	printc("\nUNIT TEST (CBUFP)\n");

	for (i = 0 ; i < MAX_CBUFPS ; i++) {
		/* //int sz = ((rand() % MAX_CBUFP_SZ) + 1) & (PAGE_SIZE - 1);
		 * Can't use arbitrary sizes for cbufps, there aren't enough
		 * freelists available. */
		int sz = MAX_CBUFP_SZ;
		cbs[i] = unit_cbufp_alloc(sz);
		assert(cbs[i]);
		unit_cbufp_deref(cbs[i]);
	}
	printc("UNIT TEST PASSED: alloc->deref\n");

	for (i = 0 ; i < MAX_CBUFPS ; i++) {
		int sz = MAX_CBUFP_SZ;
		cbs[i] = unit_cbufp_alloc(sz);
	}
	for (i = 0 ; i < MAX_CBUFPS ; i++) {
		unit_cbufp_deref(cbs[i]);
	}
	printc("UNIT TEST PASSED: N alloc -> N dealloc\n");

	for (i = 0 ; i < MAX_CBUFPS ; i++) {
		int sz = MAX_CBUFP_SZ;
		cbs[i] = unit_cbufp_alloc(sz);
		szs[i] = sz;
		bufs[i] = cbufp2buf(cbs[i], sz);
		assert(bufs[i]);
		cbufp_send_deref(cbs[i]);
		unit_cbufp2buf(cbs[i], sz); /* error path test */
	}
	printc("UNIT TEST PASSED: N alloc + cbufp2buf\n");

	for (i = 0 ; i < MAX_CBUFPS ; i++) {
		bufs[i] = cbufp2buf(cbs[i], sz);
		assert(bufs[i]);
		cbufp_deref(cbs[i]);
	}
	printc("UNIT TEST PASSED: N cached cbufp2buf\n");

	for (i = 0 ; i < MAX_CBUFPS ; i++) {
		unit_cbufp_deref(cbs[i]);
	}
	printc("UNIT TEST PASSED: N deallocs\n");

	printc("UNIT TEST (CBUFP) ALL PASSED\n");
}

void cos_init(void)
{
	printc("\nUNIT TEST (CBUF & CBUFP)\n");
	cbuf_tests();
	cbufp_tests();
	printc("UNIT TEST (CBUF & CBUFP) ALL PASSED\n");
	return;
}
