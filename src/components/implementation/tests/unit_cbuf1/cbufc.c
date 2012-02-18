#include <cos_component.h>
#include <print.h>
#include <stdlib.h> 		/* rand */
#include <unit_cbuf.h>
#include <cbuf.h>

//#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif

#define MAX_CBUFS   200
#define MAX_CBUF_SZ 4096

void cos_init(void)
{
	cbuf_t cbs[MAX_CBUFS];
	int szs[MAX_CBUFS];
	char *bufs[MAX_CBUFS];
	int i;

	printc("UNIT TEST Unit tests for cbufs...\n");

	for (i = 0 ; i < MAX_CBUFS ; i++) {
		int sz = (rand() % MAX_CBUF_SZ) + 1;
		bufs[i] = cbuf_alloc(sz, &cbs[i]);
		printv("UNIT TEST alloc %d -> %p\n", sz, bufs[i]);
		assert(bufs[i]);
		cbuf_free(bufs[i]);
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
		cbuf_free(bufs[i]);
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
		cbuf_free(bufs[i]);
		printv("UNIT TEST free %p\n", bufs[i]);
	}
	printc("UNIT TEST PASSED: N deallocs\n");

	printc("UNIT TEST ALL PASSED\n");
	
	return;
}
