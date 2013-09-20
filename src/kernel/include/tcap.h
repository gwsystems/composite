/**
 * Copyright (c) 2013 by The George Washington University, Jakob Kaivo
 * and Gabe Parmer.
 * 
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 * 
 * Initial Author: Jakob Kaivo, jkaivo@gwu.edu, 2013.
 * Additional: Gabe Parmer, gparmer@gwu.edu, 2013.
 */

#ifndef TCAP_H
#define TCAP_H

#include "shared/cos_types.h"

#ifndef TCAP_MAX_DELEGATIONS
#define TCAP_MAX_DELEGATIONS 8
#endif

#define TCAP_NACTIVATIONS 1

/* 
 * This is a reference to a tcap, and the epoch tracks which
 * "generation" of the tcap is valid for this reference.  This enables
 * fast, and O(1) revocation (simply increase the epoch in the tcap).
 */
struct tcap_ref {
	struct tcap *tcap;
	/* if the epoch in the tcap is != epoch, the reference is invalid */
	u32_t        epoch; 	
};

struct tcap_budget {
	/* overrun due to tick granularity can result in cycles < 0 */
        s64_t cycles;	
};

struct tcap_sched_info {
	struct spd *sched;
	u16_t prio;
};


#define TCAP_PRIO_MIN ((1UL<<16)-1)
#define TCAP_PRIO_MAX (1UL)

struct tcap {
	/* 
	 * The budget might be from a shared pool in which case budget
	 * refers to the parent tcap, or it might be segregated in
	 * this capability in which case budget = this.
	 */
	struct tcap_ref    budget;
	struct tcap_budget budget_local; /* if we have a partitioned budget */
	u32_t              epoch;	 /* when a tcap is deallocated, epoch++ */
	u8_t               ndelegs, sched_info;
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

static inline int
tcap_is_allocated(struct tcap *t)
{ return t->ndelegs != 0; }

static inline struct tcap_sched_info *
tcap_sched_info(struct tcap *t)
{ return &t->delegations[t->sched_info]; }

/* 
 * Delegaters might be deallocated and reused, so a pointer is not
 * sufficient to validate if the tcap is valid.  Epochs are maintained
 * for each "version" of a tcap, and when dereferenced, we check the
 * version.
 */
static inline struct tcap *
tcap_deref(struct tcap_ref *r)
{
	struct tcap *tc;

	if (unlikely(!r->tcap)) return NULL;
	tc = r->tcap;
	if (unlikely(!tcap_is_allocated(tc) || tc->epoch != r->epoch)) return NULL;
	return tc;
}

static inline void
tcap_ref_create(struct tcap_ref *r, struct tcap *t)
{
	r->tcap  = t;
	r->epoch = t ? t->epoch : 0;
}

/* 
 * Return 0 if budget left, 1 otherwise.  Consume budget from both the
 * local and the parent budget.
 */
static inline int
tcap_consume(struct tcap *t, u32_t cycles)
{
	struct tcap *bc;
	int left = 0;

	assert(t);
	if (!TCAP_RES_IS_INF(t->budget_local.cycles)) {
		t->budget_local.cycles -= cycles;
		if (t->budget_local.cycles <= 0) {
			t->budget_local.cycles = 0;
			left = 1;
		}
	}
	
	bc = tcap_deref(&t->budget);
	if (unlikely(!bc)) return left;
	if (!TCAP_RES_IS_INF(bc->budget_local.cycles)) {
		bc->budget_local.cycles -= cycles;
		if (bc->budget_local.cycles <= 0) left = -1;
	}

	return left;
}

static inline long long
tcap_remaining(struct tcap *t)
{
	struct tcap *bc;
	long long bp, bl;

	assert(t);
	bc = tcap_deref(&t->budget);
	if (unlikely(!bc)) return 0;
	bp = bc->budget_local.cycles;
	bl = t->budget_local.cycles;

	return bp < bl ? bp : bl;
}

tcap_t       tcap_id(struct tcap *t);
int          tcap_is_root(struct tcap *t);
struct tcap *tcap_get(struct spd *c, tcap_t id);
void         tcap_spd_init(struct spd *c);
struct tcap *tcap_split(struct tcap *t, s64_t cycles, u16_t prio);
int tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc, 
		  s64_t cycles, u16_t prio);
int tcap_delegate(struct tcap *tcapdst, struct tcap *tcapsrc,
		  s64_t cycles, int prio);
int tcap_delete_all(struct spd *spd);
int tcap_higher_prio(struct thread *activated, struct thread *curr);
int tcap_receiver(struct thread *t, struct tcap *tcapdst);
void tcap_elapsed(struct thread *t, unsigned int cycles);
int tcap_bind(struct thread *t, struct tcap *tcap);

int tcap_root(struct spd *s);
void tcap_root_rem(struct spd *dst);
int tcap_root_alloc(struct spd *dst, struct tcap *from, int prio, int cycles);
void tcap_root_yield(struct spd *s);
struct thread *tcap_tick_handler(void);

#endif	/* TCAP_H */
