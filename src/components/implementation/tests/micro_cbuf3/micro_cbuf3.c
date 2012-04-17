#include <cos_component.h>
#include <print.h>
#include <cbuf.h>
#include <sched.h>

#define SZ 4096

//#define MEASURE_STK_PIP
#define MEASURE_CBUF_PIP

#ifdef MEASURE_STK_PIP                  // change back to stkmgr !!!!
void call_cbuf3(int low, int high)
{
	if (cos_get_thd_id() == low) {
		sched_wakeup(cos_spd_id(), high);
	}

	return; 
}

#endif

#ifdef MEASURE_CBUF_PIP    // change policy ; change to simple stack; change interface and Makefile (MANDITORY LIB)
void call_cbuf3(int low, int high)
{
	cbuf_t cbt;
	void *mt;

	cbt = cbuf_null();

	u64_t start = 0, end = 0;

	if (cos_get_thd_id() == high) rdtscll(start);
	mt = cbuf_alloc(SZ, &cbt);
	if (cos_get_thd_id() == high) {
		rdtscll(end);
		printc("cost of cached cbufPIP %llu cycs\n", end-start);
	}
	
	if (cos_get_thd_id() == low) {
		printc("to wake up thd %d\n",high);
		sched_wakeup(cos_spd_id(), high);
	}

	printc("I am thd %d\n", cos_get_thd_id());
	cbuf_free(mt);

	return; 
}
#endif
