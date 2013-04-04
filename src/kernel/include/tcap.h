/**
 * Copyright (c) 2013 by The George Washington University.
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

#ifndef MAX_DELEGATIONS
#define MAX_DELEGATIONS 8
#endif

#define MAX_TCAP 16

struct tcap_ref {
	struct tcap *tcap;
	/* if the epoch in the tcap is != epoch, the reference is invalid */
	u32_t        epoch; 	
};


/* A tcap's maximum rate */
struct budget {
        s32_t cycles;		/* overrun due to tick granularity can result in cycles < 0 */
	u32_t expiration; 	/* absolute time (in ticks) */
};

struct tcap {
	/* 
	 * The budget might be from a shared pool in which case budget
	 * refers to the parent tcap, or it might be segregated in
	 * this capability in which case budget = this.
	 */
	struct tcap_ref budget;
	struct budget   budget_local; /* if we have a partitioned budget */
	u32_t           epoch;	      /* when a tcap is deallocated, epoch++ */
	u16_t           ndelegs, prio, cpuid;
	struct spd     *sched;

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
	struct tcap_delegation {
		u16_t           prio;
		struct tcap_ref tcap;
		struct spd *sched;
	} delegations[MAX_DELEGATIONS];

	struct tcap *freelist;
};

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
	if (unlikely(tc->epoch != r->epoch)) return NULL;
	return tc;
}

static inline void
tcap_ref_create(struct tcap_ref *r, struct tcap *t)
{
	r->tcap  = t;
	r->epoch = t->epoch;
}

/* return 0 if budget left, 1 otherwise */
static inline int
tcap_consume(struct tcap *t, u32_t cycles)
{
	struct tcap *bc;

	assert(t);
	bc = tcap_deref(&t->budget);
	if (unlikely(!bc)) return 1;
	bc->budget_local.cycles -= cycles;

	return bc->budget_local.cycles <= 0;
}

static inline int
tcap_remaining(struct tcap *t)
{
	struct tcap *bc;
	struct budget *b;
	extern u32_t ticks;

	bc = tcap_deref(&t->budget);
	if (unlikely(!bc)) return 0;
	b = &bc->budget_local;
	if (b->cycles <= 0 || b->expiration < ticks) return 0;

	return b->cycles;
}

int tcap_id(struct tcap *t);
struct tcap *tcap_split(struct spd *c, struct tcap *t, int pooled, s32_t cycles, 
			u32_t expiration, u16_t prio);
int tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc, 
		  s32_t cycles, u32_t expiration, u16_t prio, int pooled);
int tcap_delegate(struct tcap *tcapdst, struct tcap *tcapsrc, struct spd *c, 
		  int pooled, int cycles, int expiration, int prio);
int tcap_delete(struct spd *s, struct tcap *tcap);
int tcap_higher_prio(struct thread *activated, struct thread *curr);

#endif	/* TCAP_H */
