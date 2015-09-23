/**
 * Copyright (c) 2013 by The George Washington University, Jakob Kaivo
 * and Gabe Parmer.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial Author: Jakob Kaivo, jkaivo@gwu.edu, 2013.
 * Additional: Gabe Parmer, gparmer@gwu.edu, 2013; Eric Armbrust, earmbrust@gwu.edu, 2015.
 */

#include "include/tcap.h"
#include "include/thd.h"
#include "include/shared/cos_types.h"
#include "include/chal/deps.h"

#ifdef LINUX_TESTS
#undef get_cpuid
#define get_cpuid() 0
#endif

/* FIXME: Fix for multi-core support */
static tcap_uid_t tcap_uid = 0;

/* Fill in default "safe" values */
static void
tcap_init(struct tcap *t)
{
	t->ndelegs              	= 1;
	t->budget.cycles  		= 0LL;
	t->cpuid                	= get_cpuid();
	t->delegations[0].prio  	= TCAP_PRIO_MAX;
	t->delegations[0].tcap_uid 	= tcap_uid;
	t->curr_sched_off           	= 0;
	t->refcnt 			= 0;
	tcap_uid++;
}

static int
tcap_delete(struct tcap *s, struct tcap *tcap)
{
	assert(s && tcap);
	assert(tcap < &s->tcaps[TCAP_MAX] && tcap >= &s->tcaps[0]);
	/* Can't delete your persistent tcap! */
	if (s == tcap) return -1;
	/* tcap still holds a reference to a child */
	if (tcap->refcnt) return -1;
	memset(&tcap->budget, 0, sizeof(struct tcap_budget));
	memset(tcap->delegations, 0, sizeof(struct tcap_sched_info) * TCAP_MAX_DELEGATIONS);
	tcap->ndelegs    = tcap->cpuid = 0;
	tcap->curr_sched_off = 0;
	s->refcnt--;

	return 0;
}

/*
 * Pass in the budget destination and source, as well as the number of
 * cycles to be transferred.
 */
static inline int
__tcap_budget_xfer(struct tcap_budget *bd, struct tcap_budget *bs, tcap_res_t cycles, int pooled)
{
	assert(cycles >= 0);
	if (TCAP_RES_IS_INF(cycles)) {
		if (unlikely(!TCAP_RES_IS_INF(bs->cycles) && !pooled)) return -1;
		bd->cycles = TCAP_RES_INF;
		return 0;
	}
	if (unlikely(cycles > bs->cycles)) return -1;
	if (!TCAP_RES_IS_INF(bd->cycles)) bd->cycles += cycles;
	if (!TCAP_RES_IS_INF(bs->cycles)) bs->cycles -= cycles;
	return 0;
}

/*
 * This all makes the assumption that the first entry in the delegate
 * array for the tcap is the root capability.
 */
static int
__tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc,
		tcap_res_t cycles, tcap_prio_t prio, int pooled)
{
	assert(tcapdst && tcapsrc);
	if (unlikely(tcapsrc->cpuid != get_cpuid()    ||
		     tcapdst->cpuid != tcapsrc->cpuid || cycles < 0)) return -1;
	if (!prio)   prio   = tcap_sched_info(tcapsrc)->prio;
	if (!cycles) cycles = tcapsrc->budget.cycles;

	if (unlikely(__tcap_budget_xfer(&tcapdst->budget, &tcapsrc->budget, cycles, pooled))) return -1;
	if (pooled) {
		/* inherit the (possibly inherited) budget */
		memcpy(&tcapdst->budget, &tcapsrc->budget, sizeof(struct tcap_budget));
	} else {
		struct tcap *bcs, *bcd;

		bcs = tcapsrc->pool;
		bcd = tcapdst->pool;
		if (unlikely(!bcs || !bcd)) goto undo_xfer;
		if (__tcap_budget_xfer(&bcd->budget, &bcs->budget, cycles, pooled)) goto undo_xfer;
	}
	tcap_sched_info(tcapdst)->prio = prio;
	return 0;
undo_xfer:
	__tcap_budget_xfer(&tcapsrc->budget, &tcapdst->budget, cycles, pooled);
	return -1;
}

/*
 * pooled = 1 -> share the budget with t.  Otherwise:
 * cycles = 0 means remove all cycles from existing tcap
 *
 * prio = 0 denotes inheriting the priority (lower values = higher priority)
 *
 * Error conditions include t->cycles < cycles, prio < t->prio
 * (ignoring values of 0).
 */
int
tcap_split(capid_t cap, struct tcap *tcap_new, capid_t capin, struct captbl *ct, capid_t compcap, struct cap_tcap *tcap_src, tcap_split_flags_t flags)
{
	struct tcap *b, *t;
	struct cap_tcap *tc;
	struct cap_comp *compc;
	int ret;
	tcap_uid_t c;

	t = tcap_src->tcap;

	compc = (struct cap_comp *)captbl_lkup(ct, compcap);
	if (unlikely(!compc || compc->h.type != CAP_COMP)) return -EINVAL;

	tc = (struct cap_tcap *)__cap_capactivate_pre(ct, cap, capin, CAP_TCAP, &ret);
	if (!tc) return ret;

	assert(t);
	if (t->cpuid != get_cpuid()) return -ENOENT;
	c             = tcap_sched_info(t)->tcap_uid;
	assert(c);
	b             = t->pool;
	if (unlikely(!b))  return -ENOENT;

	tcap_init(tcap_new);
	if (flags == TCAP_SPLIT_POOL) tcap_ref_create(tcap_new, tcap_new);
	else tcap_ref_create(tcap_new, b);
	tcap_new->ndelegs    = t->ndelegs;
	tcap_new->curr_sched_off = t->curr_sched_off;
	memcpy(tcap_new->delegations, t->delegations, sizeof(struct tcap_sched_info) * t->ndelegs);

	tc->tcap  = tcap_new;
	tc->cpuid = tcap_new->cpuid;
	__cap_capactivate_post(&tc->h, CAP_TCAP);

	return 0;
}

/*
 * Do the delegation chains for the destination and the source tcaps
 * enable time to be transferred from the source to the destination?
 *
 * Specifically, can we do the delegation given the delegation history
 * of both of the tcaps?  If the source has been delegated to by
 * schedulers that the destination has not, then we would be moving
 * time from a more restricted environment to a less restricted one.
 * Not OK as this will leak time in a manner that the parent could not
 * control.  If any of the source delegations have a lower numerical
 * priority in the destination, we would be leaking time to a
 * higher-priority part of the system, thus heightening the status of
 * that time.
 *
 * Put simply, this checks:
 * dst \subset src \wedge
 * \forall_{s \in dst, s != src[src_sched]} sched.prio <=
 *                                          src[sched.id].prio
 */
static int
__tcap_legal_transfer_delegs(struct tcap_sched_info *dst_ds, int d_nds,
			     struct tcap_sched_info *src_ds, int s_nds, tcap_uid_t suid)
{
	int i, j;

	for (i = 0, j = 0 ; i < d_nds && j < s_nds ; ) {
		struct tcap_sched_info *s, *d;

		d = &dst_ds[i];
		s = &src_ds[j];
		/*
		 * Ignore the current scheduler; it is allowed to
		 * change its own tcap's priorities, and is not in its
		 * own delegation list.
		 */
		if (d->tcap_uid == suid) {
			i++;
			continue;
		}
		if (d->tcap_uid == s->tcap_uid) {
			if (d->prio < s->prio) return -1;
			/* another option is to _degrade_ the
			 * destination by manually lower the
			 * delegation's priority.  However, I think
			 * having a more predictable check is more
			 * important, rather than perhaps causing
			 * transparent degradation of priority. */
			i++;
		}
		/* OK so far, look at the next comparison */
		j++;
	}
	if (j == s_nds && i != d_nds) return -1;

	return 0;
}

static inline int
__tcap_legal_transfer(struct tcap *dst, struct tcap *src)
{
	return __tcap_legal_transfer_delegs(dst->delegations, dst->ndelegs,
					    src->delegations, src->ndelegs, tcap_sched_info(src)->tcap_uid);
}

int
tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc,
	      tcap_res_t cycles, tcap_prio_t prio)
{
	if (__tcap_legal_transfer(tcapdst, tcapsrc)) return -1;
	return __tcap_transfer(tcapdst, tcapsrc, cycles, prio, 1);
}

int
tcap_delegate(struct tcap *dst, struct tcap *src, tcap_res_t cycles, int prio)
{
	struct tcap_sched_info deleg_tmp[TCAP_MAX_DELEGATIONS];
	int ndelegs, i, j;
	tcap_uid_t d, s;
	int si = -1;

	assert(dst && src);
	if (unlikely(dst->ndelegs >= TCAP_MAX_DELEGATIONS)) {
		printk("tcap %x already has max number of delgations.\n",
			dst->delegations[0].tcap_uid);
		return -1;
	}
	d = tcap_sched_info(dst)->tcap_uid;
	s = tcap_sched_info(src)->tcap_uid;
	if (d == s) return -1;
	if (!prio) prio = tcap_sched_info(src)->prio;

	for (i = 0, j = 0, ndelegs = 0 ;
	     i < dst->ndelegs || j < src->ndelegs ;
	     ndelegs++) {
		struct tcap_sched_info *n, t;

		if (i == dst->ndelegs) {
			n = &src->delegations[j++];
		} else if (j == src->ndelegs) {
			n = &dst->delegations[i++];
		} else if (dst->delegations[i].tcap_uid < src->delegations[j].tcap_uid) {
			n = &dst->delegations[i++];
		} else if (dst->delegations[i].tcap_uid > src->delegations[j].tcap_uid) {
			n = &src->delegations[j++];
		} else {	/* same scheduler */
			assert(dst->delegations[i].tcap_uid == src->delegations[j].tcap_uid);
			memcpy(&t, &src->delegations[j], sizeof(struct tcap_sched_info));
			if (dst->delegations[i].prio > t.prio) {
				t.prio = dst->delegations[i].prio;
			}
			n = &t;
			i++;
			j++;
		}

		if (n->tcap_uid == s)                   n->prio = prio;
		if (ndelegs == TCAP_MAX_DELEGATIONS) return -1;
		memcpy(&deleg_tmp[ndelegs], n, sizeof(struct tcap_sched_info));
		if (d == deleg_tmp[ndelegs].tcap_uid)   si  = ndelegs;
	}

	if (__tcap_transfer(dst, src, cycles, 0, 0)) return -1;
	memcpy(dst->delegations, deleg_tmp, sizeof(struct tcap_sched_info) * ndelegs);
	/* can't delegate to yourself, thus 2 schedulers involved */
	assert(ndelegs >= 2);
	dst->ndelegs    = ndelegs;
	assert(si != -1);
	dst->curr_sched_off = si;

	/*
	 * If the component is not already a listed root, add it.
	 * Otherwise add it to the front of the list (the current tcap
	 * has permissions to execute now, so that should be
	 * transitively granted to this scheduler.
	 */
	//TODO: Add root tcap logic.
	return 0;
}

int
tcap_merge(struct tcap *dst, struct tcap *rm)
{
	struct tcap *td, *tr;

	tr = rm->pool;
	td = dst->pool;
	if (!tr || !td) return -1;

	if (tr != td &&
	    tcap_transfer(dst, rm, 0, tcap_sched_info(dst)->prio)) return -1;
	if (tr != td &&
	    tcap_transfer(dst, rm, 0, tcap_sched_info(dst)->prio)) return -1;
	if (tcap_delete(rm->pool, rm)) return -1;

	return 0;
}

static int timer_activation = 0;


/*
 * Is the newly activated thread of a higher priority than the current
 * thread?  Of all of the code in tcaps, this is the fast path that is
 * called for each interrupt and asynchronous thread invocation.
 */

int
tcap_higher_prio(struct tcap *a, struct tcap *c)
{
	int i, j;

	if (timer_activation) return 1;

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
			//same shared scheduler!
			if (a->delegations[i].prio > c->delegations[j].prio) return 0;
			i++;
			j++;
		}
	}

	return 1;
}

/*
 * FIXME: ugly hack to ensure that the timer ticks are always chosen
 * to execute.
 */
void
tcap_timer_choose(int c)
{ timer_activation = c; }
