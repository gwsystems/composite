#include <cos_component.h>
#include <print.h>

#include <sched.h>
#include <pong.h>
 
#define ITER 256
u64_t meas[ITER];

void cos_init(void)
{
	u64_t start, end, avg = 0, dev = 0;
	int i;

	printc("Starting Invocations.\n");

	for (i = 0 ; i < ITER ; i++) {
		rdtscll(start);
		call();
		rdtscll(end);
		meas[i] = end-start;
	}

	for (i = 0 ; i < ITER ; i++) avg += meas[i];
	avg /= ITER;
	printc("avg %lld\n", avg);

	for (i = 0 ; i < ITER ; i++) {
		u64_t diff = (meas[i] > avg) ? 
			meas[i] - avg : 
			avg - meas[i];
		dev += diff;
	}
	dev /= ITER;
	printc("deviation %lld\n", dev);
	
//	printc("%d invocations took %lld\n", ITER, end-start);
	return;
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
