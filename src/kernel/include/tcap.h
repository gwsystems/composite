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

#ifndef TCAP_MAX_DELEGATIONS
#define TCAP_MAX_DELEGATIONS 8
#endif

#define TCAP_NACTIVATIONS 1

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


#define TCAP_PRIO_MIN ((1UL<<16)-1)
#define TCAP_PRIO_MAX (1UL)

struct tcap {
	/*
	 * The budget might be from a shared pool in which case budget
	 * refers to the parent tcap, or it might be segregated in
	 * this capability in which case budget = this.
	 */
	struct tcap 	   *pool;
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

static inline struct tcap_sched_info *
tcap_sched_info(struct tcap *t)
{ return &t->delegations[t->curr_sched_off]; }

static inline void
tcap_ref_take(struct tcap *t)
{ t->refcnt++; }

static inline void
tcap_ref_release(struct tcap *t)
{ t->refcnt--; }

static inline void
tcap_ref_create(struct tcap *r, struct tcap *t)
{
	r->pool = t;
	tcap_ref_take(t);
}

/*
 * Return 0 if budget left, 1 if the tcap is out of budget, and -1 if
 * the pool is out.  Consume budget from both the local and the parent
 * budget.
 */
static inline int
tcap_consume(struct tcap *t, tcap_res_t cycles)
{
	struct tcap *bc;
	int left = 0;

	assert(t);
	if (!TCAP_RES_IS_INF(t->budget.cycles)) {
		t->budget.cycles -= cycles;
		if (t->budget.cycles <= 0) {
			t->budget.cycles = 0;
			left = 1;
		}
	}

	bc = t->pool;
	if (bc == t) return left;
	if (!TCAP_RES_IS_INF(bc->budget.cycles)) {
		bc->budget.cycles -= cycles;
		if (bc->budget.cycles <= 0) left = -1;
	}

	/* TODO: Add removal from global list of pools if we've consumed all cycles. */
	return left;
}

static inline struct tcap *
tcap_current(struct cos_cpu_local_info *cos_info)
{ return (struct tcap *)(cos_info->curr_tcap); }

int tcap_split(struct captbl *ct, capid_t cap, capid_t capin, struct tcap *tcap_new,
	       capid_t srctcap_cap, tcap_split_flags_t flags, int init);
int tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc,
		  tcap_res_t cycles, tcap_prio_t prio);
int tcap_delegate(struct tcap *tcapdst, struct tcap *tcapsrc,
		  s64_t cycles, int prio);
int tcap_merge(struct tcap *dst, struct tcap *rm);
int tcap_higher_prio(struct tcap *a, struct tcap *c);

struct thread *tcap_tick_handler(void);
void tcap_timer_choose(int c);

#endif	/* TCAP_H */
