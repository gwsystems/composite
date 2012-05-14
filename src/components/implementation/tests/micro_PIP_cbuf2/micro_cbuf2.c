#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <micro_cbuf3.h>

#define ITER 2000

void call_cbuf2(void)
{
	static int first = 0;
	static int low = 0, high = 0;
	
	if (first == 0){
		high = cos_get_thd_id();
		low = high + 1;
		first = 1;
	}
	int j = 0;
	while(j++ < ITER){
		if (cos_get_thd_id() == high) {
			sched_block(cos_spd_id(), 0);
		}
		call_cbuf3(low, high);
	}
	return;
}

/* measure stack w/PIP cost */
void call_stk(void)
{
	static int first = 0;
	static int low = 0, high = 0;
	
	if (first == 0 ){
		high = cos_get_thd_id();
		low = high + 1;
		first = 1;
	}

	u64_t start = 0, end = 0;

	int j = 0;
	while(j++ < ITER){
		if (cos_get_thd_id() == high) {
			/* printc("%d to block...\n", cos_get_thd_id()); */
			sched_block(cos_spd_id(), 0);
			/* printc("%d is woken up...\n", cos_get_thd_id()); */
			rdtscll(start);
		}

		call_lower(low, high);
 
		if (cos_get_thd_id() == high) {
			rdtscll(end);
			printc("C: %llu\n", end-start);
		}
	}
	return;
}
