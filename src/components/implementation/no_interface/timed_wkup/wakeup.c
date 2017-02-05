#include <cos_component.h>
#include <print.h>

#include <sched.h>

void cos_init(void)
{
	u64_t t = 0;

	printc("Starting wakeup latency tester.\n");
	if (sched_timeout_thd(cos_spd_id())) BUG();

	while (1) {
		u32_t tt;
		sched_timeout(cos_spd_id(), 1);
		rdtscll(t);
		tt = sched_timer_stopclock();
		printc("timed_wait %u\n", ((u32_t)t)-tt);
	}

	return;
}
