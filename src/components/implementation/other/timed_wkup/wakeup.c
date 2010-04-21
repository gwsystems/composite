#include <cos_component.h>
#include <print.h>

#include <sched.h>


void cos_init(void)
{
	u64_t t = 0;
	int odd = 1;

	/* 
	 * The following code goes in sched_timer_tick(void)
	 * 

	 static u64_t t = 0;
	 static int odd = 0;


		if (odd) {
			rdtscll(t);
			odd = 0;
		} else {
			printc("t @ %lld\n", t);
			odd = 1;
		}
	 */

	printc("Starting wakeup latency tester.\n");
	if (sched_timeout_thd(cos_spd_id())) BUG();

	while (1) {
		sched_timeout(cos_spd_id(), 1);
		if (odd) {
			rdtscll(t);
			odd = 0;
		} else {
			printc("w @ %lld\n", t);
			odd = 1;
		}
	}

	return;
}
