#include <cos_component.h>
#include <print.h>

#include <sched.h>
#include <pong.h>
 
#define ITER 10000

void cos_init(void)
{
	u64_t start, end;
	int i;

	printc("Starting Invocations.\n");

	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		call();
	}
	rdtscll(end);

	printc("%d invocations took %lld\n", ITER, end-start);
	return;
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
