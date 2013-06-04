#include <cos_component.h>
#include <print.h>

#include <sched.h>
#include <pong.h>
 
#define ITER (1024)
u64_t meas[ITER];

volatile int blocked = -1;

void cos_init(void)
{
	u64_t start, end, avg, tot = 0, dev = 0;
	int i, j;

	static int first = 1, second = 1;





	return;





	if (first) {
		union sched_param sp;
		first = 0;
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 20;
		if (sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();

		return;
	}
	/* printc("thd %d, core %d delay....\n", cos_get_thd_id(), cos_cpuid()); */
	/* volatile int a = 10000, b = 10000, c = 1; */
	/* for ( ; a > 0; a--) { */
	/* 	b = 10000; */
	/* 	for( ; b > 0; b --) { */
	/* 		c = 0; */
	/* 	} */
	/* } */

	printc("thd %d, core %ld going to call wakeup!\n", cos_get_thd_id(), cos_cpuid());
	sched_wakeup(cos_spd_id(), 20);
	printc("thd %d, core %ld called wakeup!\n", cos_get_thd_id(), cos_cpuid());
	return;

	call();			/* get stack */
	printc("cpu %ld from ping\n",cos_cpuid());
	printc("Starting %d Invocations.\n", ITER);

	for (i = 0 ; i < ITER ; i++) {
		rdtscll(start);
//		cos_send_ipi(i, 0, 0, 0);
		call();
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
