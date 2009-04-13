#include "include/measurement.h"
#include <linux/kernel.h>

#ifdef MEASUREMENTS

struct cos_meas_struct cos_measurements[COS_MEAS_MAX_SIZE] = 
{
	{.type = MEAS_CNT, .description = "normal component invocations"},
	{.type = MEAS_CNT, .description = "bootstrapping upcalls"},
	{.type = MEAS_CNT, .description = "self (effectless) thread switch"},
	{.type = MEAS_CNT, .description = "switch thread call with outdated info"},	
	{.type = MEAS_CNT, .description = "cooperative thread switch"},
	{.type = MEAS_CNT, .description = "thread switch to preempted thd"},
	{.type = MEAS_CNT, .description = "interrupt with attempted brand made"},
	{.type = MEAS_CNT, .description = "immediately executed branded upcalls"},
	{.type = MEAS_CNT, .description = "delayed brand upcall execution (e.g. less urg)"},
	{.type = MEAS_CNT, .description = "incriment pending brands (delay brand)"},
	{.type = MEAS_CNT, .description = "completed brands -> upcall scheduler"},
	{.type = MEAS_CNT, .description = "brand upcall scheduler -> exec pending"},
	{.type = MEAS_CNT, .description = "brand upcall scheduler -> tailcall completion"},
	{.type = MEAS_CNT, .description = "completed brands -> schedule preempted thd"},
	{.type = MEAS_CNT, .description = "completed brands -> execute pending upcall thd"},
	{.type = MEAS_CNT, .description = "brand should be made, but delayed due to net xmit"},
	{.type = MEAS_CNT, .description = "branded upcalls finished"},
	{.type = MEAS_CNT, .description = "interrupted user-level"},
	{.type = MEAS_CNT, .description = "interrupted kern-level"},
	{.type = MEAS_CNT, .description = "interrupted cos thread"},
	{.type = MEAS_CNT, .description = "interrupted other thread"},
	{.type = MEAS_CNT, .description = "interrupt in between the sti and sysexit on syscall ret"},
	{.type = MEAS_CNT, .description = "composite page fault"},
	{.type = MEAS_CNT, .description = "linux page fault"},
	{.type = MEAS_CNT, .description = "unknown fault"},
	{.type = MEAS_CNT, .description = "mpd alloc"},
	{.type = MEAS_CNT, .description = "mpd subordinate"},
	{.type = MEAS_CNT, .description = "mpd split mpd reuse"},
	{.type = MEAS_CNT, .description = "mpd free"},
	{.type = MEAS_CNT, .description = "mpd refcnt increase"},
	{.type = MEAS_CNT, .description = "mpd refcnt decrease"},
	{.type = MEAS_CNT, .description = "mpd ipc refcnt increase"},
	{.type = MEAS_CNT, .description = "mpd ipc refcnt decrease"},
	{.type = MEAS_CNT, .description = "page table allocation"},
	{.type = MEAS_CNT, .description = "page table free"},
	{.type = MEAS_CNT, .description = "memory page grant"},
	{.type = MEAS_CNT, .description = "memory page revoke"},
	{.type = MEAS_CNT, .description = "atomic operation rollback"},
	{.type = MEAS_CNT, .description = "atomic stale lock request"},
	{.type = MEAS_CNT, .description = "atomic lock request"},
	{.type = MEAS_CNT, .description = "atomic unlock request"},
	{.type = MEAS_CNT, .description = "received a network packet"},
	{.type = MEAS_CNT, .description = "make a brand for a network packet"},
	{.type = MEAS_CNT, .description = "network brand resulted in data being transferred"},
	{.type = MEAS_CNT, .description = "network brand failed: ring buffer full"},
	{.type = MEAS_CNT, .description = "networking packet transmit"},
	{.type = MEAS_CNT, .description = "pending notification HACK"},
	{.type = MEAS_CNT, .description = "attempted switching to inactive upcall"},
	{.type = MEAS_CNT, .description = "update event state to PENDING"},
	{.type = MEAS_CNT, .description = "update event state to ACTIVE"},
	{.type = MEAS_CNT, .description = "update event state to READY"},

	{.type = MEAS_STATS, .description = "delay between a brand and when upcall is executed"},
	{.type = MEAS_STATS, .description = "delay between a brand and when upcall is terminated"},
	{.type = MEAS_STATS, .description = "delay between uc term/pend and pending upcall completion"}
};

void cos_meas_init(void)
{
	int i;

	for (i = 0; i < COS_MEAS_MAX_SIZE ; i++) {
		cos_measurements[i].cnt = cos_measurements[i].meas = 0;
		cos_measurements[i].tot = cos_measurements[i].max = 0;
		cos_measurements[i].min = ~(0ULL);
	}

	return;
}

void cos_meas_report(void)
{
	int i;

	printk("cos: Measurements:\n");
	for (i = 0 ; i < COS_MEAS_MAX_SIZE ; i++) {
		switch (cos_measurements[i].type) {
		case MEAS_CNT:
			printk("cos: %8lld : %s\n", 
			       cos_measurements[i].cnt, cos_measurements[i].description);
			break;
		case MEAS_STATS:
			printk("cos: %56s : avg: %lld/%lld, max: %lld, min: %lld\n",
			       cos_measurements[i].description, 
			       cos_measurements[i].tot, cos_measurements[i].cnt, 
			       cos_measurements[i].max, cos_measurements[i].min);
			break;
		default:
			printk("cos: unknown type for %d of %d", i, cos_measurements[i].type);
		}
	}

	return;
	
}

#endif
