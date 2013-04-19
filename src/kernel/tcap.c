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

void 
tcap_spd_init(struct spd *c)
{
	int i;
	struct tcap *t;

	c->tcap_freelist = &c->tcaps[1];
	c->ntcaps        = 1;
	for (i = 1 ; i < TCAP_MAX ; i++) {
		t            = &c->tcaps[i];
		t->allocated = t->ndelegs = t->epoch = t->cpuid = 0;
		t->freelist  = &c->tcaps[i+1];
		t->sched     = c;
	}
	t           = &c->tcaps[TCAP_MAX-1];
	t->freelist = NULL;

	/* initialize tcap */
	t = &c->tcaps[0];
	tcap_ref_create(&t->budget, t);
	t->ndelegs                 = 0;
	t->epoch                   = 0;
	t->budget_local.expiration = ~0;
	t->budget_local.cycles     = INT_MAX;
	t->cpuid                   = get_cpuid();
	t->prio                    = TCAP_PRIO_MIN;
	t->allocated               = 1;
	t->sched                   = c;
	t->freelist                = NULL;
}

int
tcap_id(struct tcap *t)
{
	assert(t && t->sched);
	return t - t->sched->tcaps;
}

struct tcap *
tcap_get(struct spd *c, tcap_t id)
{
	struct tcap *t;

	assert(c);
	if (unlikely(id >= TCAP_MAX)) return NULL;
	t = &c->tcaps[id];
	if (unlikely(!t->allocated))  return NULL;
	return t;
}

/* 
 * Set thread t to be bound to tcap.  Its execution will proceed with
 * that tcap from this point on.  This is most useful for interrupt
 * threads.
 */
int 
tcap_bind(struct thread *t, struct tcap *tcap)
{
	assert(t && tcap && tcap->sched);
	if (!thd_scheduled_by(t, tcap->sched)) return -1;
	tcap_ref_create(&t->tcap_active, tcap);
	return 0;
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
		tcapdst->budget_local.expiration  = expiration;
		b->cycles                        -= cycles;
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
	if (t->cpuid != get_cpuid()) return NULL;
	n = c->tcap_freelist;
	if (unlikely(!n)) return NULL;

	if (unlikely(tcap_transfer(n, t, cycles, expiration, prio, pooled))) {
		return NULL;
	}

	/* transfer successful, commit to the change */
	n->allocated     = 1;
	c->tcap_freelist = n->freelist;
	n->freelist      = NULL;
	n->ndelegs       = t->ndelegs;
	n->cpuid         = get_cpuid();
	memcpy(n->delegations, t->delegations, 
	       sizeof(struct tcap_delegation) * t->ndelegs);

	return n;
}

/* 
 * Which tcap should receive delegations while executing in thread t?
 */
int 
tcap_receiver(struct thread *t, struct tcap *tcap)
{
	assert(t && tcap);
	if (!thd_scheduled_by(t, tcap->sched)) return -1;
	tcap_ref_create(&t->tcap_receiver, tcap);
	return 0;
}

int
tcap_delegate(struct tcap *tcapdst, struct tcap *tcapsrc, struct spd *c, 
	      int pooled, int cycles, int expiration, int prio)
{
	struct tcap_delegation *td;
	int i;

	assert(tcapdst && tcapsrc);
	if (unlikely(tcapdst->ndelegs >= TCAP_MAX_DELEGATIONS)) {
		printk("tcap %x already has max number of delgations.\n", 
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
	assert(tcap < &s->tcaps[TCAP_MAX] && tcap >= &s->tcaps[0]);
	/* Can't delete your persistent tcap! */
	if (&s->tcaps[0] == tcap) return -1;
	tcap->epoch++; 		/* now all references to the tcap are invalid */
	tcap->freelist   = s->tcap_freelist;
	s->tcap_freelist = tcap;
	tcap->allocated  = 0;
	memset(&tcap->budget_local, 0, sizeof(struct budget));
	memset(tcap->delegations, 0, sizeof(struct tcap_delegation) * TCAP_MAX_DELEGATIONS);
	tcap->ndelegs = tcap->cpuid = 0;

	return 0;
}

int
tcap_delete_all(struct spd *spd)
{
	int i;
	
	assert(spd);
	for (i = 0 ; i < spd->ntcaps-1 ; i++) {
		tcap_delete(spd, &(spd->tcaps[i]));
	}
	spd->ntcaps = 0;

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
		/* 
		 * Shouldn't require this as we're executing on this
		 * tcap now.  There is actually a significant
		 * performance hit for having to do this.  The memory
		 * touched (epoch in the tcap) is on a different
		 * cache-line, and in a different page.  This can
		 * cause data-cache and TLB misses.
		 *
		 * If we don't check this now, then between the time
		 * that we're given this tcap, and when the interrupt
		 * arrives, the tcap can be revoked, and we won't
		 * detect it here.
		 */
		//if (!tcap_deref(&a->delegations[i].tcap)) return 0; 
		if (c->delegations[i].prio <= a->delegations[i].prio) {
			return 0;
		}
	}
	if (a->sched == c->sched && c->prio <= a->prio) return 0;

	return 1;
}

void
tcap_elapsed(struct thread *t, unsigned int cycles)
{
	struct tcap *tc;

	tc = tcap_deref(&t->tcap_active);
	printk("tcap_elapsed: thread %d, cycles %ld.\n", 
	       thd_get_id(t), cycles);
	assert(tc);
	tcap_consume(tc, cycles);
}

static struct spd *tcap_fountain;

int
tcap_fountain(struct spd *c)
{
	assert(c);
	tcap_fountain = c;
	return 0;
}

int 
tcap_tick_process(void)
{
	struct tcap *tc;
	extern u32_t cyc_per_tick;
	extern u32_t ticks;

	if (unlikely(!tcap_fountain)) return -1;
	c = tcap_fountain;
	tc                          = &c->tcaps[0];
	tc->budget_local.cycles     = cyc_per_tick;
	tc->budget_local.expiration = ticks + 1;

	return 0;
}
