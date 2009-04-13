#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#define MEASUREMENTS
#define MEASUREMENTS_STATS

typedef enum { 
	/* counters */
	COS_MEAS_INVOCATIONS = 0,
	COS_MEAS_UPCALLS,
	COS_MEAS_SWITCH_SELF,
	COS_MEAS_SWITCH_OUTDATED,
	COS_MEAS_SWITCH_COOP,
	COS_MEAS_SWITCH_PREEMPT,
	COS_MEAS_INT_PREEMPT,
	COS_MEAS_BRAND_UC,
	COS_MEAS_BRAND_DELAYED,
	COS_MEAS_BRAND_PEND,
	COS_MEAS_BRAND_COMPLETION_UC,
	COS_MEAS_BRAND_COMPLETION_PENDING,
	COS_MEAS_BRAND_COMPLETION_TAILCALL,
	COS_MEAS_BRAND_SCHED_PREEMPTED,
	COS_MEAS_BRAND_PEND_EXECUTE,
	COS_MEAS_BRAND_DELAYED_UC,
	COS_MEAS_FINISHED_BRANDS,
	COS_MEAS_INT_PREEMPT_USER,
	COS_MEAS_INT_PREEMPT_KERN,
	COS_MEAS_INT_COS_THD,
	COS_MEAS_INT_OTHER_THD,
	COS_MEAS_INT_STI_SYSEXIT,
	COS_PG_FAULT,
	COS_LINUX_PG_FAULT,
	COS_UNKNOWN_FAULT,
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
	COS_MAP_GRANT,
	COS_MAP_REVOKE,
	COS_MEAS_ATOMIC_RBK,
	COS_MEAS_ATOMIC_STALE_LOCK,
	COS_MEAS_ATOMIC_LOCK,
	COS_MEAS_ATOMIC_UNLOCK,
	COS_MEAS_PACKET_RECEPTION,
	COS_MEAS_PACKET_BRAND,
	COS_MEAS_PACKET_BRAND_SUCC,
	COS_MEAS_PACKET_BRAND_FAIL,
	COS_MEAS_PACKET_XMIT,
	COS_MEAS_PENDING_HACK,
	COS_MEAS_UPCALL_INACTIVE,
	COS_MEAS_EVT_PENDING,
	COS_MEAS_EVT_ACTIVE,
	COS_MEAS_EVT_READY,

	/* stats */
	COS_MEAS_STATS_UC_EXEC_DELAY,
	COS_MEAS_STATS_UC_TERM_DELAY,
	COS_MEAS_STATS_UC_PEND_DELAY,
	COS_MEAS_MAX_SIZE
} cos_meas_t;


typedef enum {
	MEAS_CNT,
	MEAS_STATS,
	MEAS_MAX
} meas_type_t;

#ifdef MEASUREMENTS
void cos_meas_init(void);
void cos_meas_report(void);

struct cos_meas_struct {
	meas_type_t type;
	char *description;
	unsigned long long cnt, meas, tot, min, max;
};

extern struct cos_meas_struct cos_measurements[COS_MEAS_MAX_SIZE];
static inline void cos_meas_event(cos_meas_t type)
{
	/* silent error is better here as we wish to avoid
	 * conditionals in hotpaths to check for the return value */
	if (type >= COS_MEAS_MAX_SIZE) return;
	if (cos_measurements[type].type != MEAS_CNT) return;
	cos_measurements[type].cnt++;

	return;
}

#ifdef MEASUREMENTS_STATS
#define cos_meas_rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

/* 
 * Without overwrite, the timestamp will always be taken. With it, the
 * timestamp will be taken only if it is initially 0.
 */
static inline void cos_meas_stats_start(cos_meas_t type, int overwrite)
{
	unsigned long long ts;

	if (type >= COS_MEAS_MAX_SIZE) return;
	if (cos_measurements[type].type != MEAS_STATS) return;
	if (cos_measurements[type].meas != 0 && 0 == overwrite) return;

	cos_meas_rdtscll(ts);
	cos_measurements[type].meas = ts;
}

/* 
 * If reset is set, then the timestamp is reset to 0, otherwise it is
 * updated to the current reading.  If the timestamp is zero, then no
 * recording will be made as no matching stats_start was made.
 */
static inline void cos_meas_stats_end(cos_meas_t type, int reset)
{
	unsigned long long ts, diff;
	struct cos_meas_struct *cms;

	if (type >= COS_MEAS_MAX_SIZE) return;
	if (cos_measurements[type].type != MEAS_STATS) return;
	if (cos_measurements[type].meas == 0) return;

	cms = &cos_measurements[type];
	cos_meas_rdtscll(ts);
	diff = ts - cms->meas;
	cms->meas = ts;
	cms->tot += diff;
	if (diff < cms->min) cms->min = diff;
	if (diff > cms->max) cms->max = diff;
	cms->cnt++;

	if (reset) cms->meas = 0;
}
#else 
#define cos_meas_stats_start(t, o)
#define cos_meas_stats_end(t, r)
#endif	/* MEASUREMENTS_STATS */

#else

#define cos_meas_event(t)
#define cos_meas_init()
#define cos_meas_report()
#define cos_meas_stats_start(t, o)
#define cos_meas_stats_end(t, r)

#endif

#endif
