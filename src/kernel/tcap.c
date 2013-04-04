/**
 * Copyright (c) 2013 by The George Washington University.
 * 
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 * 
 * Initial Author: Jakob Kaivo, jkaivo@gwu.edu, 2013.
 * Additional: Gabe Parmer, gparmer@gwu.edu, 2013.
 */

#include "include/tcap.h"
#include "include/thread.h"

int
tcap_id(struct tcap *t)
{
	assert(t && t->sched);
	return t - t->sched->tcaps;
}

/* 
 * This all makes the assumption that the first entry in the delegate
 * array for the tcap is the root capability (the fountain of time).
 */

int
tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc, 
	      s32_t cycles, u32_t expiration, u16_t prio, int pooled)
{
	struct budget *b;
	struct tcap *bc;

	assert (tcapdst && tcapsrc);

	if (unlikely(tcapsrc->cpuid != get_cpuid() ||
		     tcapdst->cpuid != tcapsrc->cpuid)) return -1;
	bc = tcap_deref(&tcapsrc->budget);
	if (unlikely(!bc)) return -1;
	b = &bc->budget_local;

	/* If the specified values are "use default" */
	if (!pooled) {
		if (!cycles)     cycles     = b->cycles;
		if (!expiration) expiration = b->expiration;
		if (unlikely(b->cycles < cycles || 
			     b->expiration > expiration)) return -1;
	}
	if (!prio) prio = tcapsrc->prio;
	if (unlikely(prio < tcapsrc->prio)) return -1;

	/* update the source tcap */
	if (!pooled) {
		tcap_ref_create(&tcapdst->budget, tcapdst);
		tcapdst->budget_local.cycles     += cycles;
		b->cycles                        -= cycles;
		tcapdst->budget_local.expiration  = expiration;
	} else {
		tcap_ref_create(&tcapdst->budget, tcapsrc);
	}
	tcapdst->prio     = prio;

	return 0;
}

/* 
 * pooled = 1 -> share the budget/expiration with t.  Otherwise:
 * cycles = 0 means remove all cycles from existing tcap
 * expiration = 0 denotes inheriting the expiration
 * 
 * prio = 0 denotes inheriting the priority (lower values = higher priority)
 *
 * Error conditions include t->cycles < cycles, t->expiration >
 * expiration, prio < t->prio (ignoring values of 0).
 */
struct tcap *
tcap_split(struct spd *c, struct tcap *t, int pooled, 
	   s32_t cycles, u32_t expiration, u16_t prio)
{
	struct tcap *n;
	assert(c && t);

	n = c->tcap_freelist;
	if (unlikely(!n)) return NULL;
	n->cpuid = get_cpuid();

	if (unlikely(tcap_transfer(n, t, cycles, expiration, prio, pooled))) {
		return NULL;
	}

	/* transfer successful, commit to the change */
	c->tcap_freelist  = n->freelist;
	n->freelist       = NULL;
	n->ndelegs        = t->ndelegs;
	memcpy(n->delegations, t->delegations, 
	       sizeof(struct tcap_delegation) * t->ndelegs);

	return n;
}

int
tcap_delegate(struct tcap *tcapdst, struct tcap *tcapsrc, struct spd *c, 
	      int pooled, int cycles, int expiration, int prio)
{
	struct tcap_delegation *td;
	int i;

	assert(tcapdst && tcapsrc);
	if (unlikely(tcapdst->ndelegs >= MAX_DELEGATIONS)) {
		printk ("tcap %x already has max number of delgations.\n", 
			tcap_id(tcapdst));
		return -1;
	}

	if (unlikely(tcap_transfer(tcapdst, tcapsrc, cycles, expiration, prio, pooled))) {
		return -1;
	}
	
	td = &tcapdst->delegations[tcapdst->ndelegs];
	for (i = 0 ; i < tcapdst->ndelegs ; i++) {
		if (tcapdst->delegations[i].sched == c) {
			td = &tcapdst->delegations[i];
			break;
		}
	}
	td->prio  = tcapsrc->prio;
	td->sched = c;
	tcap_ref_create(&td->tcap, tcapsrc);
	tcapdst->ndelegs++;

	return 0;
}

int 
tcap_delete(struct spd *s, struct tcap *tcap)
{
	assert(s && tcap);
	assert(tcap < &s->tcaps[MAX_TCAP] && tcap >= &s->tcaps[0]);
	/* Can't delete your persistent tcap! */
	if (&s->tcaps[0] == tcap) return -1;
	tcap->epoch++;
	tcap->freelist   = s->tcap_freelist;
	s->tcap_freelist = tcap;

	return 0;
}

/* 
 * Is the newly activated thread of a higher priority than the current
 * thread?  Fast path called for each interrupt.
 */
int tcap_higher_prio(struct thread *activated, struct thread *curr)
{
	struct tcap *a, *c, *b;
	int i;
	assert(activated && curr);

	a = tcap_deref(&activated->tcap_active);
	c = tcap_deref(&curr->tcap_active);
	/* invalid/inactive tcaps? */
	if (unlikely(!a)) return 0;
	if (unlikely(!c)) return 1;

	b = tcap_deref(&c->budget);
	if (unlikely(!b || b->budget_local.cycles <= 0)) return 0;
	for (i = 0 ; i < a->ndelegs && i < c->ndelegs ; i++) {
		if (i > 0 && 
		    a->delegations[i].sched != c->delegations[i].sched) break;
		if (!tcap_deref(&a->delegations[i].tcap)) return 0; 
		if (c->delegations[i].prio <= a->delegations[i].prio) {
			return 0;
		}
	}
	if (a->sched == c->sched && c->prio <= a->prio) return 0;

	return 1;
}
