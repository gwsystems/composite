#include <cos_component.h>
#include <print.h>
#include <stdlib.h> 		/* rand */
#include <cbuf.h>
#include <sched.h>

//#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif

#define MAX_SZ 4096

/* Test CBUF PIP COST

   change policy to 1
   change to simple stack
   change interface and Makefile (MANDITORY LIB) 
   use simple stack in s_stub.S
*/
void call_cbuf3(int low, int high)
{
	cbuf_t cbt;
	void *mt;
	unsigned int sz;
	sz = (rand() % MAX_SZ) + 1;

	cbt = cbuf_null();

	u64_t start = 0, end = 0;

	if (cos_get_thd_id() == high) rdtscll(start);
	printv("thd %d getting cbufs\n", cos_get_thd_id());
	mt = cbuf_alloc_ext(sz, &cbt, CBUF_TMEM);
	printv("thd %d got cbufs\n", cos_get_thd_id());
	if (cos_get_thd_id() == high) {
		rdtscll(end);
		printc("cbufPIP C: %llu cycs\n", end-start);
	}
	
	if (cos_get_thd_id() == low) {
		sched_wakeup(cos_spd_id(), high);
	}

	printv("thd %d freeing cbufs\n", cos_get_thd_id());
	cbuf_free(cbt);
	printv("thd %d freed cbufs\n", cos_get_thd_id());

	return; 
}

/* Test STACK PIP cost

   change policy to 1   
   using stkmgr instead simple stk and thdpool_1 for PIP
   do not use simple stack in s_stub.S 

*/

void call_lower(int low, int high)
{
	if (cos_get_thd_id() == low) {
		/* printc("%d to wake up...%d\n", cos_get_thd_id(), high); */
		sched_wakeup(cos_spd_id(), high);
		/* printc("%d get running\n", cos_get_thd_id()); */
	}
	return;
}
