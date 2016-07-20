/**
 * Copyright (c) 2013 by The George Washington University, Jakob Kaivo
 * and Gabe Parmer.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial Author: Jakob Kaivo, jkaivo@gwu.edu, 2013.
 * Additional: Gabe Parmer, gparmer@gwu.edu, 2013
 */

#include "include/tcap.h"
#include "include/thd.h"
#include "include/shared/cos_types.h"
#include "include/chal/defs.h"

struct tcap_percore {
	struct tcap transient_tcap;
	tcap_uid_t  tcap_uid;
} CACHE_ALIGNED;

static struct tcap_percore __tcap_percore[NUM_CPU] PAGE_ALIGNED;

static inline struct tcap *
tcap_transient_get(void)
{ return &__tcap_percore[get_cpuid()].transient_tcap; }

static inline tcap_uid_t *
tcap_uid_get(void)
{ return &__tcap_percore[get_cpuid()].tcap_uid; }

static inline int
tcap_ispool(struct tcap *t)
{ return t == t->pool; }

/* Fill in default "safe" values */
static void
__tcap_init(struct tcap *t, tcap_prio_t prio)
{
	tcap_uid_t *uid = tcap_uid_get();

	t->budget.cycles           = 0LL;
	t->cpuid                   = get_cpuid();
	t->ndelegs                 = 1;
	t->delegations[0].tcap_uid = (*uid)++;
	t->curr_sched_off          = 0;
	t->refcnt                  = 1;
	t->pool                    = t;
	tcap_setprio(t, prio);
}

static int
tcap_delete(struct tcap *tcap)
{
	assert(tcap);
	if (tcap_ref(tcap)) return -1;
	memset(&tcap->budget, 0, sizeof(struct tcap_budget));
	memset(tcap->delegations, 0, sizeof(struct tcap_sched_info) * TCAP_MAX_DELEGATIONS);
	tcap->ndelegs = tcap->cpuid = tcap->curr_sched_off = 0;
	if (tcap_ispool(tcap)) tcap_ref_release(tcap->pool);

	return 0;
}

/*
 * Pass in the budget destination and source, as well as the number of
 * cycles to be transferred.
 */
static inline int
__tcap_budget_xfer(struct tcap_budget *bd, struct tcap_budget *bs, tcap_res_t cycles)
{
	if (unlikely(TCAP_RES_IS_INF(cycles))) {
		if (unlikely(!TCAP_RES_IS_INF(bs->cycles))) return -1;
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
__tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc, tcap_res_t cycles, tcap_prio_t prio)
{
	assert(tcapdst && tcapsrc);

	if (unlikely(tcapsrc->cpuid != get_cpuid() || tcapdst->cpuid != tcapsrc->cpuid)) return -1;
	if (prio == 0)   prio   = tcap_sched_info(tcapsrc)->prio;
	if (cycles == 0) cycles = tcapsrc->budget.cycles;

	if (unlikely(__tcap_budget_xfer(&tcapdst->budget, &tcapsrc->budget, cycles))) return -1;
	tcap_sched_info(tcapdst)->prio = prio;

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
__tcap_legal_transfer(struct tcap *dst, struct tcap *src)
{
	struct tcap_sched_info *dst_ds = dst->delegations, *src_ds = src->delegations;
	int                     d_nds  = dst->ndelegs,      s_nds = src->ndelegs;
	tcap_uid_t              suid   = tcap_sched_info(src)->tcap_uid;
	int                     i, j;

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
			/*
			 * another option is to _degrade_ the
			 * destination by manually lower the
			 * delegation's priority.  However, I think
			 * having a more predictable check is more
			 * important, rather than perhaps causing
			 * transparent degradation of priority.
			 */
			i++;
		}
		/* OK so far, look at the next comparison */
		j++;
	}
	if (j == s_nds && i != d_nds) return -1;

	return 0;
}

int
tcap_transfer(struct tcap *tcapdst, struct tcap *tcapsrc, tcap_res_t cycles, tcap_prio_t prio)
{
	if (__tcap_legal_transfer(tcapdst->pool, tcapsrc->pool)) return -EINVAL;
	return __tcap_transfer(tcapdst->pool, tcapsrc->pool, cycles, prio);
}

int
tcap_activate(struct captbl *ct, capid_t cap, capid_t capin, struct tcap *tcap_new, tcap_prio_t prio)
{
	struct cap_tcap *tc;
	int ret;

	assert(tcap_new);
	__tcap_init(tcap_new, prio);

	tc = (struct cap_tcap *)__cap_capactivate_pre(ct, cap, capin, CAP_TCAP, &ret);
	if (!tc) return ret;

	tc->cpuid = tcap_new->cpuid;
	tc->tcap  = tcap_new;
	__cap_capactivate_post(&tc->h, CAP_TCAP);

	return 0;
}

void
tcap_promote(struct tcap *t, struct thread *thd)
{
	if (tcap_ispool(t)) return;
	tcap_ref_release(t->pool);
	t->arcv_ep = thd;
	t->pool    = t;
}

int
tcap_delegate(struct tcap *dst, struct tcap *src, tcap_res_t cycles, tcap_prio_t prio)
{
	/* doing this in-place is too much of a pain */
	struct tcap_sched_info deleg_tmp[TCAP_MAX_DELEGATIONS];
	int ndelegs, i, j;
	tcap_uid_t d, s;
	int si = -1;
	int ret = 0;

	assert(dst && src);
	assert(tcap_ispool(dst));
	/* we can ignore the source priority as it is overwritten by prio */
	src = src->pool;
	if (unlikely(dst->ndelegs >= TCAP_MAX_DELEGATIONS)) return -ENOMEM;

	d = tcap_sched_info(dst)->tcap_uid;
	s = tcap_sched_info(src)->tcap_uid;
	if (d == s) return -EINVAL;
	if (!prio) prio = tcap_sched_info(src)->prio;

	for (i = 0, j = 0, ndelegs = 0 ; i < dst->ndelegs || j < src->ndelegs ; ndelegs++) {
		struct tcap_sched_info *n, t;

		/* Let the branch prediction nightmare begin... */
		if (j == src->ndelegs        || dst->delegations[i].tcap_uid < src->delegations[j].tcap_uid) {
			n = &dst->delegations[i++];
		} else if (i == dst->ndelegs || dst->delegations[i].tcap_uid > src->delegations[j].tcap_uid) {
			n = &src->delegations[j++];
		} else {	/* same scheduler */
			assert(dst->delegations[i].tcap_uid == src->delegations[j].tcap_uid);
			memcpy(&t, &src->delegations[j], sizeof(struct tcap_sched_info));
			if (dst->delegations[i].prio > t.prio) t.prio = dst->delegations[i].prio;
			n = &t;
			i++;
			j++;
		}

		if (n->tcap_uid == s) n->prio = prio;
		if (unlikely(ndelegs == TCAP_MAX_DELEGATIONS)) return -ENOMEM;
		memcpy(&deleg_tmp[ndelegs], n, sizeof(struct tcap_sched_info));
		if (d == deleg_tmp[ndelegs].tcap_uid) si = ndelegs;
	}

	if (__tcap_transfer(dst, src, cycles, prio)) return -EINVAL;
	memcpy(dst->delegations, deleg_tmp, sizeof(struct tcap_sched_info) * ndelegs);
	/* can't delegate to yourself, thus 2 schedulers must be involved */
	assert(ndelegs >= 2);
	dst->ndelegs = ndelegs;
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
	if (dst == rm                                             ||
	    tcap_transfer(dst, rm, 0, tcap_sched_info(dst)->prio) ||
	    tcap_delete(rm)) return -1;

	return 0;
}

void
tcap_init(void)
{
	int i;

	for (i = 0 ; i < NUM_CPU ; i++) {
		__tcap_init(&__tcap_percore[i].transient_tcap, TCAP_PRIO_MIN);
		__tcap_percore[i].tcap_uid = 0;
	}
}
