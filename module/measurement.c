#include "include/measurement.h"
#include <linux/kernel.h>

#ifdef MEASUREMENTS

unsigned long long cos_measurements[COS_MEAS_MAX_SIZE];
char *cos_meas_descriptions[COS_MEAS_MAX_SIZE+1] = 
{
	"normal component invocations",
	"bootstrapping upcalls",
	"self (effectless) thread switch",
	"cooperative thread switch",
	"thread switch to preempted thd ",
	"interrupt with attempted brand made",
	"immediately executed branded upcalls",
	"delayed brand upcall execution (e.g. less urg)",
	"incriment pending brands (delay brand)",
	"completed brands -> upcall scheduler",
	"completed brands -> schedule preempted thd",
	"completed brands -> execute pending upcall thd",
	"branded upcalls finished",
	"interrupted user-level",
	"interrupted kern-level",
	"interrupted cos thread",
	"interrupted other thread",
	"composite page fault",
	"linux page fault",
	"unknown fault",
	"mpd alloc",
	"mpd subordinate",
	"mpd split mpd reuse",
	"mpd free",
	"mpd refcnt increase",
	"mpd refcnt decrease",
	"mpd ipc refcnt increase",
	"mpd ipc refcnt decrease",
	"page table allocation",
	"page table free",
	"memory page grant",
	"memory page revoke",
	"atomic operation rollback",
	"atomic stale lock request",
	"atomic lock request",
	"atomic unlock request",
	"received a network packet",
	"make a brand for a network packet",
	"network brand resulted in data being transferred",
	"network brand failed: ring buffer full",
	"pending notification HACK"
	""
};

void cos_meas_init(void)
{
	int i;

	for (i = 0; i < COS_MEAS_MAX_SIZE ; i++) {
		cos_measurements[i] = 0;
	}

	return;
}

void cos_meas_report(void)
{
	int i;

	printk("cos: Measurements:\n");
	for (i = 0 ; i < COS_MEAS_MAX_SIZE ; i++) {
		printk("cos: %50s : %lld\n", 
		       cos_meas_descriptions[i], cos_measurements[i]);
	}

	return;
	
}

#endif
