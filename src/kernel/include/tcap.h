/**
 * Copyright (c) 2013 by The George Washington University, Jakob Kaivo
 * and Gabe Parmer.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial Author: Jakob Kaivo, jkaivo@gwu.edu, 2013
 * Additional: Gabe Parmer, gparmer@gwu.edu, 2013
 */

#ifndef TCAP_H
#define TCAP_H

#include "shared/cos_types.h"
#include "component.h"
#include "thd.h"

#ifndef TCAP_MAX_DELEGATIONS
#define TCAP_MAX_DELEGATIONS 16
#endif

struct cap_tcap {
	struct cap_header h;
	struct tcap *tcap;
	cpuid_t cpuid;
} __attribute__((packed));

/*
 * This is a reference to a tcap, and the epoch tracks which
 * "generation" of the tcap is valid for this reference.  This enables
 * fast, and O(1) revocation (simply increase the epoch in the tcap).
 */

struct tcap_budget {
	/* overrun due to tick granularity can result in cycles < 0 */
        tcap_res_t cycles;
};

struct tcap_sched_info {
	tcap_uid_t  tcap_uid;
	tcap_prio_t prio;
};

struct tcap {
	/*
	 * The budget might be from a shared pool in which case budget
	 * refers to the parent tcap, or it might be segregated in
	 * this capability in which case budget = this.
	 */
	struct tcap 	   *pool;
	struct thread      *arcv_ep; /* if ispool, this is the arcv endpoint */
	u32_t 		   refcnt;
	struct tcap_budget budget; /* if we have a partitioned budget */
	u8_t               ndelegs, curr_sched_off;
	u16_t              cpuid;

	/*
	 * Which chain of temporal capabilities resulted in this
	 * capability's access, and what access is granted? We might
	 * want to "cache" the priority here when we have strictly
	 * fixed priorities, thus "priority".
	 *
	 * Note that we don't simply have a struct tcap * here as that
	 * tcap might be outdated (deallocated/reallocated).  Instead,
	 * we record the path to access the tcap (component, and
	 * offset), and the epoch of the "valid" tcap.
	 *
	 * Why the complexity?  Revocation for capability-based
	 * systems is difficult.  This enables revocation by simply
	 * incrementing the epoch of a tcap.  If it is outdated, then
	 * we assume it is of the lowest-priority.
	 */
	struct tcap_sched_info delegations[TCAP_MAX_DELEGATIONS];
	struct tcap           *next, *prev;
	struct tcap           *freelist;
};

void tcap_init(void);
int tcap_activate(struct captbl *ct, capid_t cap, capid_t capin, struct tcap *tcap_new);
int tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc, tcap_res_t cycles, tcap_prio_t prio);
int tcap_delegate(struct tcap *tcapdst, struct tcap *tcapsrc, tcap_res_t cycles, int prio);
int tcap_merge(struct tcap *dst, struct tcap *rm);
void tcap_promote(struct tcap *t, struct thread *thd);

struct thread *tcap_tick_handler(void);
void tcap_timer_choose(int c);

static inline struct tcap_sched_info *
tcap_sched_info(struct tcap *t)
{ return &t->delegations[t->curr_sched_off]; }

static inline void
tcap_ref_take(struct tcap *t)
{ t->refcnt++; }

static inline void
tcap_ref_release(struct tcap *t)
{ t->refcnt--; }

static inline int
tcap_ref(struct tcap *t)
{ return t->refcnt; }


/*
 * Return 0 if budget left, 1 if the tcap is out of budget, and -1 if
 * the pool is out.  Consume budget from both the local and the parent
 * budget.
 */
static inline tcap_res_t
tcap_consume(struct tcap *t, tcap_res_t cycles)
{
	assert(t);
	t = t->pool;
	if (TCAP_RES_IS_INF(t->budget.cycles)) return 0;
	t->budget.cycles -= cycles;
	if (t->budget.cycles <= 0) {
		tcap_res_t left = cycles - t->budget.cycles;

		t->budget.cycles = 0;

		return left;
	}
	/*
	 * TODO: Add removal from global list of pools and declassify
	 * if we've consumed all cycles.
	 */
	return 0;
}

static inline int
tcap_expended(struct tcap *t)
{ return t->pool->budget.cycles == 0; }

static inline struct tcap *
tcap_current(struct cos_cpu_local_info *cos_info)
{ return (struct tcap *)(cos_info->curr_tcap); }

/* Get the current tcap _and_ update its cycle count */
static inline struct tcap *
tcap_current_update(struct cos_cpu_local_info *cos_info)
{
	struct tcap *t;
	tcap_res_t cycles, overshoot;

	t                = tcap_current(cos_info);
	cycles           = tsc();
	overshoot        = tcap_consume(t, cycles - cos_info->cycles);
	/* TODO: use overshoot somehow? return to caller? */
	cos_info->cycles = cycles;

	return t;
}

static inline void
tcap_setprio(struct tcap *t, tcap_prio_t p)
{
	assert(t);
	t->delegations[t->curr_sched_off].prio = p;
}

/*
 * Is the newly activated thread of a higher priority than the current
 * thread?  Of all of the code in tcaps, this is the fast path that is
 * called for each interrupt and asynchronous thread invocation.
 */
static inline int
tcap_higher_prio(struct tcap *a, struct tcap *c)
{
	int i, j;
	tcap_prio_t ap, cp, ap_pool, cp_pool;
	int ret = 0;

	if (tcap_expended(a)) return 0;

	/* Use the priorities of the tcaps, but the delegations of the pool */
	ap      = tcap_sched_info(a)->prio;
	cp      = tcap_sched_info(c)->prio;
	a       = a->pool;
	c       = c->pool;
	ap_pool = tcap_sched_info(a)->prio;
	cp_pool = tcap_sched_info(c)->prio;
	tcap_sched_info(a)->prio = ap;
	tcap_sched_info(c)->prio = cp;

	for (i = 0, j = 0 ; i < a->ndelegs && j < c->ndelegs ; ) {
		/*
		 * These cases are for the case where the tcaps don't
		 * share a common scheduler (due to the partial order
		 * of schedulers), or different interrupt bind points.
		 */
		if (a->delegations[i].tcap_uid > c->delegations[j].tcap_uid) {
			j++;
		} else if (a->delegations[i].tcap_uid < c->delegations[j].tcap_uid) {
			i++;
		} else {
			/* same shared scheduler! */
			if (a->delegations[i].prio > c->delegations[j].prio) goto fixup;
			i++;
			j++;
		}
	}
	ret = 1;
fixup:
	tcap_sched_info(a)->prio = ap_pool;
	tcap_sched_info(c)->prio = cp_pool;

	return ret;
}

#endif	/* TCAP_H */
