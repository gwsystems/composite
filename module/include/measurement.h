#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#define MEASUREMENTS

enum { COS_MEAS_SWITCH_COOP,
       COS_MEAS_SWITCH_PREEMPT,
       COS_MEAS_INT_PREEMPT,
       COS_MEAS_INVOCATIONS,
       COS_MEAS_UPCALLS,
       COS_MEAS_BRAND_UC,
       COS_MEAS_BRAND_PEND,
       COS_MEAS_INT_PREEMPT_USER,
       COS_MEAS_INT_PREEMPT_KERN,
       COS_MEAS_INT_COS_THD,
       COS_MEAS_OTHER_THD,
       COS_PG_FAULT,
       COS_LINUX_PG_FAULT,
       COS_MPD_ALLOC,
       COS_MPD_SUBORDINATE,
       COS_MPD_SPLIT_REUSE,
       COS_MPD_FREE,
       COS_MPD_REFCNT_INC,
       COS_MPD_REFCNT_DEC,
       COS_MPD_IPC_REFCNT_INC,
       COS_MPD_IPC_REFCNT_DEC,
       COS_ALLOC_PGTBL,
       COS_FREE_PGTBL,
       COS_MEAS_MAX_SIZE,
};

#ifdef MEASUREMENTS
void cos_meas_init(void);
void cos_meas_report(void);

extern unsigned long long cos_measurements[COS_MEAS_MAX_SIZE];
static inline void cos_meas_event(unsigned int type)
{
	/* silent error is better here as we wish to avoid
	 * conditionals in hotpaths to check for the return value */
	if (type >= COS_MEAS_MAX_SIZE)
		return;

	cos_measurements[type]++;

	return;
}

#else

#define cos_meas_event(t)
#define cos_meas_init()
#define cos_meas_report()

#endif

#endif
