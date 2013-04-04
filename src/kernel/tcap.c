#include "tcap.h"
#include "thread.h"

/* 
 * This all makes the assumption that the first entry in the delegate
 * array for the tcap is the root capability (the fountain of time).
 */

int
tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc, 
	      s32_t cycles, u32_t expiration, u16_t prio, int pooled)
{
	struct budget *b;
	struct tcap *bc, *n;

	assert (tcapdst && tcapsrc);

	if (unlikely(tcapsrc->cpuid != get_cpuid() ||
		     tcapdst->cpuid != get_cpuid())) return -1;
	bc = tcap_deref(tcapsrc->budget);
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
	if (unlikely(prio < tcapsrc->prio ||
		     !c->tcap_freelist)) return -1;

	/* update the source tcap */
	if (!pooled) {
		tcap_ref_create(tcapdst->budget, tcapdst);
		tcapdst->budget_local.cycles     += cycles;
		b->cycles                        -= cycles;
		tcapdst->budget_local.expiration  = expiration;
	} else {
		tcap_ref_create(tcapdst->budget, tcapsrc);
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

	if (unlikely(tcap_transfer(n, t, cycles, expiration, prio, pooled))) {
		return NULL;
	}

	c->tcap_freelist  = n->freelist;
	n->freelist       = NULL;

	n->ndelegs = t->ndelegs;
	n->cpuid   = get_cpuid();

	memcpy(n->delegations, t->delegations, 
	       sizeof(struct tcap_delegation) * t->ndelegs);

	return n;
}

int
tcap_delegate(struct tcap *tcapdst, struct tcap *tcapsrc, struct spd *c, 
	      int pooled, int cycles, int expiration, int prio)
{
	struct tcap_delegate *td;
	int i;

	assert(tcapdst && tcapsrc);
	if (unlikely(tcapdst->ndelegs >= MAX_DELEGATIONS)) {
		printk ("tcap %x already has max number of delgations.\n", tcap);
		return -1;
	}

	if (unlikely(tcap_transfer(tcapdst, tcapsrc, cycles, expiration, prio, pooled))) {
		return -1;
	}
	
	td = &tcapdst->delegations[tcap->ndelegs];
	for (i = 0 ; i < tcap->ndelegs ; i++) {
		if (tcap->delegations[i].sched == c) {
			td = &tcap->delegations[i];
			break;
		}
	}
	td->prio  = tcapsrc->prio;
	td->sched = comp;
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
	tcap->freelist = s->tcap_freelist;
	s->tcap_freelist = tcap;

	return 0;
}

/* 
 * Is the newly activated thread of a higher priority than the current
 * thread?
 */
int tcap_higher_prio(struct thread *activated, struct thread *curr)
{
	struct tcap *a, *c;
	struct budget *b;
	int i;
	assert(activated && curr);

	a = tcap_ref(&activated->tcap_active);
	c = tcap_ref(&curr->tcap_active);
	if (unlikely(!a)) return 0;
	if (unlikely(!c)) return 1;

	b = tcap_ref(&activated->budget);
	if (!b) return 0;
	for (i = 0 ; i < activated->ndelegs && i < curr->ndelegs ; i++) {
		if (i > 0 && activated->delegations[i].sched != curr->delegations[i].sched) break;
		if (!tcap_deref(&activated->delegations[i].tcap)) return 0; 
		if (curr->delegations[i].prio >= activated->delegations[i].prio ) {
			return 0;
		}
	}
	if (activated->sched == curr->sched &&
	    curr->prio < activated->prio) return 0;

	return 1;
}
