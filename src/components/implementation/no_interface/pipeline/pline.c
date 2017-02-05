#include <cos_component.h>
#include <timed_blk.h>
#include <evt.h>
#include <print.h>
#include <sched.h>

#define BLOCK_TIME 1
#define PLINE_LEN 2
#define LOOP_LEN  0

volatile int var = 0;

static inline void compute(void)
{
	int i;
	
	for (i = 0 ; i < LOOP_LEN ; i++) var++;
}

void start(void)
{
	unsigned long long time;
	long eid = evt_create(cos_spd_id());
	printc("event id %ld\n", eid);

	rdtscll(time);
	while (1) {
		if (eid == (PLINE_LEN-1)) {
			printc("prevs %lld\n", time);
			timed_event_block(cos_spd_id(), BLOCK_TIME);
			rdtscll(time);
		} else evt_wait(cos_spd_id(), eid);
		compute();
		if (eid > 0) evt_trigger(cos_spd_id(), eid-1);
		else {
			rdtscll(time);
			printc("done %lld\n", time);
		}
	}
}

void cos_init(void)
{
	start();
	sched_block(cos_spd_id(), 0);
}
