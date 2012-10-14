#include "include/measurement.h"
#include "include/per_cpu.h"
#include <linux/kernel.h>


#ifdef MEASUREMENTS

struct per_core_meas_struct per_core_meas[NUM_CPU];

struct cos_meas_struct measurements_desc[COS_MEAS_MAX_SIZE] = {
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
	{.type = MEAS_CNT, .description = "mpd merge"},
	{.type = MEAS_CNT, .description = "mpd split"},
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
	{.type = MEAS_CNT, .description = "break preemption chain"},

	{.type = MEAS_CNT, .description = "idle: linux idle sleep"},
	{.type = MEAS_CNT, .description = "idle: linux idle wake and run"},
	{.type = MEAS_CNT, .description = "idle: linux wakeup call"},
	{.type = MEAS_CNT, .description = "idle: recursive wakeup call"},

	{.type = MEAS_CNT, .description = "child scheduler: attempted schedule with pending cevts"},
	{.type = MEAS_CNT, .description = "scheduler: attempted schedule with pending event"},

	{.type = MEAS_STATS, .description = "delay between a brand and when upcall is executed"},
	{.type = MEAS_STATS, .description = "delay between a brand and when upcall is terminated"},
	{.type = MEAS_STATS, .description = "delay between uc term/pend and pending upcall completion"}
};

void cos_meas_init(void)
{
	int i, cpu;

	for (cpu = 0; cpu < NUM_CPU; cpu++) {
		memcpy(per_core_meas[cpu].cos_measurements, measurements_desc, sizeof(struct cos_meas_struct) * COS_MEAS_MAX_SIZE);
		for (i = 0; i < COS_MEAS_MAX_SIZE ; i++) {
			per_core_meas[cpu].cos_measurements[i].cnt = per_core_meas[cpu].cos_measurements[i].meas = 0;
			per_core_meas[cpu].cos_measurements[i].tot = per_core_meas[cpu].cos_measurements[i].max = 0;
			per_core_meas[cpu].cos_measurements[i].min = ~(0ULL);
		}
		per_core_recorded_evts[cpu].evts_head = 0;
	}

	return;
}

void cos_meas_report(void)
{
	int i, cpu;

	for (cpu = 0; cpu < NUM_CPU; cpu++) {
		if (NUM_CPU > 1 && (cpu == LINUX_CORE)) break; // no need to report the core belongs to Linux
		printk("\nCore %d Measurements:\n", cpu);
		for (i = 0 ; i < COS_MEAS_MAX_SIZE ; i++) {
			switch (per_core_meas[cpu].cos_measurements[i].type) {
			case MEAS_CNT:
				printk("cos: %8lld : %s\n", 
				       per_core_meas[cpu].cos_measurements[i].cnt, per_core_meas[cpu].cos_measurements[i].description);
				break;
			case MEAS_STATS:
				printk("cos: %56s : avg: %lld/%lld, max: %lld, min: %lld\n",
				       per_core_meas[cpu].cos_measurements[i].description, 
				       per_core_meas[cpu].cos_measurements[i].tot, per_core_meas[cpu].cos_measurements[i].cnt, 
				       per_core_meas[cpu].cos_measurements[i].max, per_core_meas[cpu].cos_measurements[i].min);
				break;
			default:
				printk("cos: unknown type for %d of %d", i, per_core_meas[cpu].cos_measurements[i].type);
			}
		}
	}

	return;
}

#endif


#ifdef COS_RECORD_EVTS

struct per_core_evt_record per_core_recorded_evts[NUM_CPU];
void event_print(void)
{
	int i, last, cpu;
	unsigned long long ts;

	cos_rdtscll(ts);
	printk("\ncos: Most recent events @ current t %llu.\n", ts);
	for (cpu = 0; cpu < NUM_CPU; cpu++) {
		if (NUM_CPU > 1 && (cpu == LINUX_CORE)) break; // no need to report the core belongs to Linux
		last = (per_core_recorded_evts[cpu].evts_head + (COS_EVTS_NUM-1)) & COS_EVTS_MASK;
		printk("\ncos: Core %d most recent events (head %d, pre %d).\n", cpu, per_core_recorded_evts[cpu].evts_head, last);
		for (i = per_core_recorded_evts[cpu].evts_head ; 1 ; i = (i+1) & COS_EVTS_MASK) {
			struct exec_evt *e = &per_core_recorded_evts[cpu].recorded_evts[i];
			printk("cos:\t%d:%s (%ld, %ld) @ %lld\n", i, e->msg, e->a, e->b, e->timestamp);
			if (i == last) break;
		}
	}
}

#endif
