#include <cos_component.h>
#include <print.h>
#include <cbuf.h>
#include <sched.h>


#define SZ 4096

//#define MEASURE_STK_PIP
#define MEASURE_CBUF_PIP

#ifdef MEASURE_STK_PIP                  // change back to stkmgr !!!!
void call_lower(int low, int high)
{
	/* printc("I am thd %d\n", cos_get_thd_id()); */
	 
	if (cos_get_thd_id() == low) {
		/* printc("p1\n"); */
		sched_wakeup(cos_spd_id(), high);
		/* printc("p2\n"); */
		/* printc("thd %d wakes up %d\n", cos_get_thd_id(), high); */
	}
	/* printc(" thd %d return \n", cos_get_thd_id()); */
	return; 
}

#endif

#ifdef MEASURE_CBUF_PIP    // change to simple stack !!!!
void call_lower(int low, int high)
{
	/* printc("I am thd %d\n", cos_get_thd_id()); */

	cbuf_t cbt;
	void *mt;

	memset(&cbt, 0 , sizeof(cbuf_t));
	cbt = cbuf_null();	

	u64_t start = 0, end = 0;

	if (cos_get_thd_id() == high) rdtscll(start);
	/* printc("thd %d is to alloc\n", cos_get_thd_id()); */
	mt = cbuf_alloc(SZ, &cbt);
	if (cos_get_thd_id() == high) {
		rdtscll(end);
		printc("cost of cached cbufPIP %llu cycs\n", end-start);
	}
	
	if (cos_get_thd_id() == low) {
		/* printc("low thd %d tries to wake up high thd %d\n", low, high); */
		sched_wakeup(cos_spd_id(), high);
		/* printc("thd %d did wake up %d\n", low, high); */
	}
	/* printc(" thd %d free mem\n", cos_get_thd_id()); */
	cbuf_free(mt);

	return; 
}
#endif
