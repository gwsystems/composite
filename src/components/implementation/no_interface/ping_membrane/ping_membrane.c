#include <cos_component.h>
#include <print.h>

#include <sched.h>
#include <membrane.h>
 
#define ITER (20000)
u64_t meas[ITER];

void cos_init(void)
{
	u64_t start, end, avg, tot = 0, dev = 0;
	int i, j;

	static int first = 1;
	
	if (first) {
		union sched_param sp;
		first = 0;
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 31;
		if (sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();
		return;
	}
	
	call_server(99,99,99,99);			/* get stack */
	return;
	printc("Core %ld: starting Invocations.\n", cos_cpuid());

	for (i = 0 ; i < ITER ; i++) {
		rdtscll(start);
		call_server(99,99,99,99);
		rdtscll(end);
		meas[i] = end-start;
	}

	for (i = 0 ; i < ITER ; i++) tot += meas[i];
	avg = tot/ITER;
	printc("avg %lld\n", avg);
	for (tot = 0, i = 0, j = 0 ; i < ITER ; i++) {
		if (meas[i] < avg*2) {
			tot += meas[i];
			j++;
		}
	}
	printc("avg w/o %d outliers %lld\n", ITER-j, tot/j);

	for (i = 0 ; i < ITER ; i++) {
		u64_t diff = (meas[i] > avg) ? 
			meas[i] - avg : 
			avg - meas[i];
		dev += (diff*diff);
	}
	dev /= ITER;
	printc("deviation^2 = %lld\n", dev);
	
//	printc("%d invocations took %lld\n", ITER, end-start);
	return;
}
