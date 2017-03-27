/*
 * Scheduler library that can be used by schedulers to manage their
 * data structures.
 *
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */
#ifndef COS_SCHEDULER_H
#define COS_SCHEDULER_H

#include <consts.h>
#include <cos_types.h>

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_list.h>

#include <sched.h>
#include <sched_hier.h>

/**************** Scheduler Util Fns *******************/

#define THD_BLOCKED    0x1
#define THD_READY      0x2
#define THD_FREE       0x4
#define THD_GRP        0x8  // is this thread a group of thds?
#define THD_MEMBER     0x10 // is this thread part of a group?
#define THD_UC_READY   0X40
#define THD_SUSPENDED  0x80 // suspended threads are similar to those
			    // that are blocked, but they may be run
			    // if they are depended on, but should be
			    // scheduled otherwise
#define THD_DEPENDENCY 0x100
#define THD_DYING      0x200
#define THD_PHANTOM    0x400 // a thread that should never be processed by policy

#define sched_thd_free(thd)          ((thd)->flags & THD_FREE)
#define sched_thd_grp(thd)           ((thd)->flags & THD_GRP)
#define sched_thd_member(thd)        ((thd)->flags & THD_MEMBER)
#define sched_thd_blocked(thd)       ((thd)->flags & THD_BLOCKED)
#define sched_thd_inactive_evt(thd)  ((thd)->flags & THD_UC_READY)
#define sched_thd_phantom(thd)       ((thd)->flags & THD_PHANTOM)
/* Thread ready: because the scheduler's thread structures might not
 * be updated to the fact that an upcall is actually active, "ready"
 * must include this discrepancy */
#define sched_thd_ready(thd)         (!sched_thd_blocked(thd) && !sched_thd_inactive_evt(thd) && !sched_thd_dying(thd))
#define sched_thd_dying(thd)         ((thd)->flags & THD_DYING)
#define sched_thd_dependent(thd)     ((thd)->flags & THD_DEPENDENCY)
#define sched_thd_suspended(thd)     ((thd)->flags & THD_SUSPENDED)

#define sched_thd_on_current_core(thd) (sched_get_thd_core(thd) == cos_cpuid())

#define SCHED_NUM_THREADS MAX_NUM_THREADS

/* cevt_t: */
#define SCHED_CEVT_OTHER     0x1
#define SCHED_CEVT_WAKE      0x2
#define SCHED_CEVT_BLOCK     0x4

typedef enum {
	NULL_EVT = 0,
	SWITCH_THD,
	ACAP_ACTIVE,
	ACAP_READY,
	ACAP_PENDING,
	ACAP_CYCLE,
	SCHED_DEPENDENCY,
	THD_BLOCK,
	THD_WAKE,
	COMP_TAKE,
	COMP_TAKE_ATTEMPT,
	COMP_TAKE_CONTENTION,
	COMP_RELEASE,
	TIMER_TICK,
	TIMER_SWITCH_LOOP,
	IDLE_SCHED,
	IDLE_SCHED_SWITCH,
	EVT_CMPLETE,
	BLOCK_LOOP,
	WAKE_LOOP,
	TIMER_LOOP,
	COMP_TAKE_LOOP,
	EVT_CMPLETE_LOOP,
	TIMEOUT_LOOP,
	IDLE_SCHED_LOOP,
	BLOCKED_DEP_RETRY,
	RETRY_BLOCK,
	BLOCKED_W_DEPENDENCY,
	DEPENDENCY_BLOCKED_THD,
	SCHED_TARGETTED_DEPENDENCY,
	PARENT_BLOCK_CHILD,
	PARENT_CHILD_EVT_OTHER,
	PARENT_CHILD_EVT_THD,
	PARENT_CHILD_DEACTIVATE,
	PARENT_CHILD_RESUME,
	PARENT_CHILD_REDUNDANT_RESUME,
	CHILD_PROCESS_EVT_IDLE,
	CHILD_PROCESS_EVT_PEND,
	CHILD_EVT_BLOCK,
	CHILD_EVT_WAKE,
	CHILD_EVT_OTHER,
	CHILD_SWITCH_THD,
	CEVT_RESCHED,
	REVT_LAST
} report_evt_t;

PERCPU_EXTERN(cos_sched_notifications);

struct sched_accounting {
	unsigned long C, T, C_used, T_exp;
	unsigned long ticks, prev_ticks;
	unsigned long long cycles, pol_cycles;
	void *private;
};

struct sched_metric {
	unsigned short int priority, urgency;
};

struct sched_thd {
	unsigned short int flags, id, evt_id;
	struct sched_accounting accounting;
	struct sched_metric metric;
	cpuid_t cpuid;
	u16_t event;
	struct sched_thd *prio_next,  *prio_prev, 
		         *sched_next, *sched_prev; /* for scheduler policy use */

	/* blocking/waking specific info: 0 = blocked, 1 = running, 2
	 * = received a wake for a block that hasn't happened yet */
	int wake_cnt;
	/* component thread is blocked in, and if there is critical
	 * section contention in a component, which is the contended
	 * component */
	spdid_t blocking_component, contended_component;
	int ncs_held;
	/* the thread that this is dependent on (via
	 * sched_block_dependency) */
	struct sched_thd *dependency_thd;
	unsigned long long block_time;

	/* scheduler hierarchy fields: child event flags */
	cevt_t cevt_flags;
	struct sched_thd *group; /* If flags & THD_MEMBER */
	spdid_t cid; /* If flags & THD_GRP */
	/* The tick value when the child was last executed, and the
	 * time when the child scheduler wishes to be woken up. */
	u64_t tick, wakeup_tick;
	/* linked list for all threads in a group: */
	struct sched_thd *next, *prev;
	/* linked list for child threads with events */
	struct sched_thd *cevt_next, *cevt_prev;

	/* If we have been killed, and are going to be reused, we have
	 * to upcall into the specified spdid...stored here. */
	/* ... and its init_data */
	spdid_t spdid;
	int init_data;
};

struct sched_crit_section {
	struct sched_thd *holding_thd;
};

struct scheduler_per_core {
        /**************** Scheduler Event Fns *******************/
	volatile u8_t cos_curr_evt;

        /************** critical section functions/state *************/
	struct sched_crit_section sched_spd_crit_sections[MAX_NUM_SPDS];

        /* --- Thread Management Utiliities --- */
	struct sched_thd *sched_thd_map[SCHED_NUM_THREADS];
	struct sched_thd sched_thds[SCHED_NUM_THREADS]; 
	struct sched_thd *sched_map_evt_thd[NUM_SCHED_EVTS];
} CACHE_ALIGNED;

void sched_init_thd(struct sched_thd *thd, unsigned short int id, int flags);
struct sched_thd *sched_alloc_thd(unsigned short int id);
struct sched_thd *sched_alloc_upcall_thd(unsigned short int thd_id);
void sched_ds_init(void);
void sched_free_thd(struct sched_thd *thd);
void sched_grp_make(struct sched_thd *thd, spdid_t chld_sched);
void sched_grp_add(struct sched_thd *grp, struct sched_thd *thd);
void sched_grp_rem(struct sched_thd *thd);

static inline struct sched_accounting *sched_get_accounting(struct sched_thd *thd)
{
	assert(!sched_thd_free(thd));
        
	return &thd->accounting;
}

static inline int sched_clear_accounting(struct sched_thd *thd)
{
	assert(!sched_thd_free(thd));

	memset(sched_get_accounting(thd), 0, sizeof(struct sched_accounting));

	return 0;
}

static inline struct sched_metric *sched_get_metric(struct sched_thd *thd)
{
	assert(!sched_thd_free(thd));

	return &thd->metric;
}

/**************** Scheduler Event Fns *******************/

typedef void (*sched_evt_visitor_t)(struct sched_thd *t, u8_t flags, u32_t cpu_consumption);
static inline int cos_sched_pending_event(void)
{
	/* struct cos_sched_events *evt; */

	/* printc("pending? %d, addr %p, cpuid %ld, thd %d\n", 
	   PERCPU_GET(cos_sched_notifications)->cos_evt_notif.pending_event, 
	   &PERCPU_GET(cos_sched_notifications)->cos_evt_notif.pending_event, 
	   cos_cpuid(), cos_get_thd_id()); */
	return PERCPU_GET(cos_sched_notifications)->cos_evt_notif.pending_event;
/*
	evt = &PERCPU_GET(cos_sched_notifications)->cos_events[cos_curr_evt];
	evt1 = COS_SCHED_EVT_FLAGS(evt) || COS_SCHED_EVT_NEXT(evt);
	assert(!(evt0 ^ evt1));
	return evt0;
*/
}

static inline void cos_sched_clear_events(void)
{
	/* printc("core %ld, thd %d clearing pending_event!!!!\n", cos_cpuid(), cos_get_thd_id()); */
	PERCPU_GET(cos_sched_notifications)->cos_evt_notif.pending_event = 0;
}

static inline void cos_sched_clear_cevts(void)
{
	PERCPU_GET(cos_sched_notifications)->cos_evt_notif.pending_cevt = 0;
}

static inline u32_t cos_sched_timer_cyc(void)
{
	u32_t t = PERCPU_GET(cos_sched_notifications)->cos_evt_notif.timer;
	PERCPU_GET(cos_sched_notifications)->cos_evt_notif.timer = 0;
	return t;
}

int cos_sched_event_to_process(void);
int cos_sched_process_events(sched_evt_visitor_t fn, unsigned int proc_amnt);
void cos_sched_set_evt_urgency(u8_t id, u16_t urgency);
short int sched_alloc_event(struct sched_thd *thd);
int sched_rem_event(struct sched_thd *thd);
int sched_share_event(struct sched_thd *n, struct sched_thd *old);

extern struct scheduler_per_core per_core_sched[NUM_CPU];

static inline struct sched_thd *sched_evt_to_thd(short int evt_id)
{
	assert(evt_id < NUM_SCHED_EVTS);
	assert(evt_id != 0);

	return per_core_sched[cos_cpuid()].sched_map_evt_thd[evt_id];
}
static inline void sched_set_thd_urgency(struct sched_thd *t, u16_t urgency)
{
	if (t->evt_id) {
		cos_sched_set_evt_urgency(t->evt_id, urgency);
	} else {
		/* Maybe assert here? */
		printc("Warning: core %ld, scheduler setting thread (id %d) urgency without event id\n", cos_cpuid(), t->id);
	}
	sched_get_metric(t)->urgency = urgency;
}

struct thd_core_mapping {
	cpuid_t thd_to_core[SCHED_NUM_THREADS];
} CACHE_ALIGNED;

struct thd_core_mapping thd_core;

static inline cpuid_t sched_get_thd_core(unsigned short int thd_id) {
	if (unlikely(thd_id >= SCHED_NUM_THREADS || thd_core.thd_to_core[thd_id] < 0)) {
		return -1;
	}
	
	return thd_core.thd_to_core[thd_id];
}

static inline int sched_set_thd_core(unsigned short int thd_id, cpuid_t cpu) {
	if (unlikely(thd_id >= SCHED_NUM_THREADS || cpu >= NUM_CPU_COS)) {
		return -1;
	}

	thd_core.thd_to_core[thd_id] = cpu;
	
	return 0;
}

/* --- Thread Id -> Sched Thread Mapping Utilities --- */
static inline struct sched_thd *sched_get_mapping(unsigned short int thd_id)
{
	if (thd_id >= SCHED_NUM_THREADS ||
	    per_core_sched[cos_cpuid()].sched_thd_map[thd_id] == NULL ||
	    (per_core_sched[cos_cpuid()].sched_thd_map[thd_id]->flags & THD_FREE)) {
		return NULL;
	}

	return per_core_sched[cos_cpuid()].sched_thd_map[thd_id];
}

static inline struct sched_thd *sched_get_current(void)
{
	unsigned short int thd_id;
	struct sched_thd *thd;

	thd_id = cos_get_thd_id();
	thd = sched_get_mapping(thd_id);
	
	return thd;
}

int sched_curr_is_IPI_handler(void);

static inline int sched_add_mapping(unsigned short int thd_id, struct sched_thd *thd)
{
	if (thd_id >= SCHED_NUM_THREADS ||
	    per_core_sched[cos_cpuid()].sched_thd_map[thd_id] != NULL) {
		return -1;
	}
	
	per_core_sched[cos_cpuid()].sched_thd_map[thd_id] = thd;

	return 0;
}

static inline void sched_rem_mapping(unsigned short int thd_id)
{
	if (thd_id >= SCHED_NUM_THREADS) return;

	per_core_sched[cos_cpuid()].sched_thd_map[thd_id] = NULL;
}

static inline int sched_is_grp(struct sched_thd *thd)
{
	assert(!sched_thd_free(thd));
	if (thd->flags & THD_GRP) {
		assert(!sched_thd_member(thd));
		return 1;
	}
	assert(!sched_thd_grp(thd));

	return 0;
}

static inline struct sched_thd *sched_get_members(struct sched_thd *grp)
{
	assert(!sched_thd_free(grp) && sched_thd_grp(grp));

	if (grp->next == grp) return NULL;
	return grp->next;
}

static inline struct sched_thd *sched_get_grp(struct sched_thd *thd)
{
	if (sched_is_grp(thd)) {
		return NULL;
	}
	return thd->group;
}

/*************** critical section functions *****************/

static inline void sched_crit_sect_init(void)
{
	int i;
	
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		struct sched_crit_section *cs = &per_core_sched[cos_cpuid()].sched_spd_crit_sections[i];

		cs->holding_thd = NULL;
	}
}

/* 
 * Dependencies can be either on a critical section in a specific
 * component, or on a specific thread that e.g. holds a lock.  FIXME:
 * This function is recursive to walk through a list of dependencies.
 * This should change.  Additionally, we do nothing to detect/prevent
 * cycles, which can cause this to infinitely regress (which, in
 * practice, will cause a fault).
 */
static inline struct sched_thd *
__sched_thd_dependency(struct sched_thd *curr)
{
	struct sched_crit_section *cs;
	spdid_t spdid;
	//struct sched_thd *dep;
	assert(curr);
	
	if (likely(!sched_thd_dependent(curr))) return NULL;
	if (curr->dependency_thd) return curr->dependency_thd;

	spdid = curr->contended_component;
	/* Horrible hack: */
	if (!spdid) goto done;
	/* If we have the dependency flag set, we should have an contended spd */
	assert(spdid < MAX_NUM_SPDS);
	
	/* We have a critical section for a spd */
	cs = &per_core_sched[cos_cpuid()].sched_spd_crit_sections[spdid];
	if (cs->holding_thd) return cs->holding_thd;
done:
	/* no more dependencies! */
	curr->flags &= ~THD_DEPENDENCY;
	curr->contended_component = 0;
	return NULL;
}

static inline struct sched_thd *
sched_thd_dependency(struct sched_thd *curr)
{
	struct sched_thd *d, *p; // dependency and prev dependency

	for (p = curr ; ((d = __sched_thd_dependency(p))) ; p = d) ;
//	if (sched_thd_blocked(p) /* || sched_thd_inactive_evt(p) */) return NULL;
	if (!sched_thd_ready(p)) return NULL;
	return p == curr ? NULL : p;
}

/* 
 * Return the thread that is holding the crit section, or NULL if it
 * is uncontested.  Assuming here we are in a critical section.
 */
static inline struct sched_thd *sched_take_crit_sect(spdid_t spdid, struct sched_thd *curr)
{
	struct sched_crit_section *cs;
	assert(spdid < MAX_NUM_SPDS);
	assert(!sched_thd_free(curr));
	assert(!sched_thd_blocked(curr));
	cs = &per_core_sched[cos_cpuid()].sched_spd_crit_sections[spdid];

	if (cs->holding_thd) {
		/* The second assumption here might be too restrictive in the future */
		assert(!sched_thd_free(cs->holding_thd));
		assert(!sched_thd_blocked(cs->holding_thd));
		/* no recursive lock taking allowed */
		assert(curr != cs->holding_thd);
		curr->contended_component = spdid;
		assert(!curr->dependency_thd);
		return cs->holding_thd;
	} 
	curr->ncs_held++;
	curr->contended_component = 0;
	cs->holding_thd = curr;
	return NULL;
}

/* Return 1 if curr does not hold the critical section, 0 otherwise */
static inline int sched_release_crit_sect(spdid_t spdid, struct sched_thd *curr)
{
	struct sched_crit_section *cs;
	assert(spdid < MAX_NUM_SPDS);
	cs = &per_core_sched[cos_cpuid()].sched_spd_crit_sections[spdid];
	assert(curr);
	assert(!sched_thd_free(curr));
	assert(!sched_thd_blocked(curr));

	/* This ostensibly should be the case */
	assert(cs->holding_thd == curr);
	assert(curr->contended_component == 0);

	cs->holding_thd = NULL;
	curr->ncs_held--;
	return 0;
}

/*************** Scheduler Synchronization Fns ***************/

#include <cos_sched_sync.h>

/*
 * This will call the switch_thread syscall after releasing the
 * scheduler lock.
 */
static inline int cos_switch_thread_release(unsigned short int thd_id, 
					    unsigned short int flags)
{
        /* This must be volatile as we must commit what we want to
	 * write to memory immediately to be read by the kernel */
	volatile struct cos_sched_next_thd *cos_next = &PERCPU_GET(cos_sched_notifications)->cos_next;

	cos_next->next_thd_id = thd_id;
	cos_next->next_thd_flags = flags;

	cos_sched_lock_release();

	/* kernel will read next thread information from cos_next */
	/* printc("core %ld: __switch_thread, thd %u, flags %u\n", cos_cpuid(), thd_id, flags); */
	int ret = cos___switch_thread(thd_id, flags); 

	return ret;
}


#endif
