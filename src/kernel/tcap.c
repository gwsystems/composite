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

/* This is jacked.  Only in here to avoid a header file circular dependency. */
void
__thd_exec_add(struct thread *t, cycles_t cycles)
{
	t->exec += cycles;
}

/*** TCap Operations ***/

static inline tcap_uid_t *
tcap_uid_get(void)
{
	return &(cos_cpu_local_info()->tcap_uid);
}

/* Fill in default "safe" values */
static void
__tcap_init(struct tcap *t)
{
	tcap_uid_t *uid = tcap_uid_get();

	t->budget.cycles           = 0LL;
	t->cpuid                   = get_cpuid();
	t->ndelegs                 = 1;
	t->delegations[0].tcap_uid = (*uid)++;
	t->curr_sched_off          = 0;
	t->refcnt                  = 1;
	t->arcv_ep                 = NULL;
	t->perm_prio               = 0;
	tcap_setprio(t, 0);
	list_init(&t->active_list, t);
}

static inline int
tcap_isactive(struct tcap *t)
{
	return t->arcv_ep != NULL;
}

static int
tcap_delete(struct tcap *tcap)
{
	struct cos_cpu_local_info *cli = cos_cpu_local_info();

	assert(tcap);
	if (tcap_ref(tcap) != 1) return -1;
	memset(&tcap->budget, 0, sizeof(struct tcap_budget));
	memset(tcap->delegations, 0, sizeof(struct tcap_sched_info) * TCAP_MAX_DELEGATIONS);
	tcap->ndelegs = tcap->cpuid = tcap->curr_sched_off = tcap->perm_prio = 0;
	if (cli->next_ti.tc == tcap) thd_next_thdinfo_update(cli, 0, 0, 0, 0);

	return 0;
}

/*
 * Pass in the budget destination and source, as well as the number of
 * cycles to be transferred.
 */
static inline int
__tcap_budget_xfer(struct tcap *d, struct tcap *s, tcap_res_t cycles)
{
	struct tcap_budget *bd, *bs;

	assert(s && d);
	assert(tcap_is_active(s));
	if (unlikely(s->cpuid != get_cpuid() || d->cpuid != s->cpuid)) return -1;

	bd = &d->budget;
	bs = &s->budget;
	if (cycles == 0) cycles= s->budget.cycles;
	if (unlikely(TCAP_RES_IS_INF(cycles))) {
		if (unlikely(!TCAP_RES_IS_INF(bs->cycles))) return -1;
		bd->cycles = TCAP_RES_INF;
		goto done;
	}
	if (unlikely(cycles > bs->cycles)) cycles = bs->cycles;
	if (!TCAP_RES_IS_INF(bd->cycles)) {
		tcap_res_t bd_cycs = bd->cycles + cycles;

		if (bd_cycs < bd->cycles || TCAP_RES_IS_INF(bd_cycs))
			bd->cycles = TCAP_RES_MAX;
		else
			bd->cycles = bd_cycs;
	}
	if (!TCAP_RES_IS_INF(bs->cycles)) bs->cycles -= cycles;
done:
	if (!tcap_is_active(d)) tcap_active_add_before(s, d);
	if (tcap_expended(s)) tcap_active_rem(s);

	return 0;
}

int
tcap_activate(struct captbl *ct, capid_t cap, capid_t capin, struct tcap *tcap_new)
{
	struct cap_tcap *tc;
	int              ret;

	assert(tcap_new);
	__tcap_init(tcap_new);

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
	if (tcap_isactive(t)) return;
	t->arcv_ep = thd;
}

int
tcap_delegate(struct tcap *dst, struct tcap *src, tcap_res_t cycles, tcap_prio_t prio)
{
	int        ndelegs, i, j;
	tcap_uid_t d, s;
	int        si  = -1;
	int        ret = 0;
	/* doing this in-place is too much of a pain */
	struct tcap_sched_info deleg_tmp[TCAP_MAX_DELEGATIONS];

	assert(dst && src);
	assert(tcap_isactive(dst));
	/* check for stack overflow */
	assert(round_to_page(&deleg_tmp[0]) == round_to_page(&deleg_tmp[TCAP_MAX_DELEGATIONS - 1]));
	if (unlikely(dst->ndelegs > TCAP_MAX_DELEGATIONS)) return -ENOMEM;
	if (unlikely(src->cpuid != get_cpuid() || dst->cpuid != src->cpuid)) return -EINVAL;
	if (unlikely(!tcap_is_active(src))) return -EPERM;

	d = tcap_sched_info(dst)->tcap_uid;
	s = tcap_sched_info(src)->tcap_uid;
	if (unlikely(dst == src)) {
		tcap_sched_info(dst)->prio = prio;
		dst->perm_prio             = prio;
		return 0;
	}
	if (!prio) prio = tcap_sched_info(src)->prio;

	for (i = 0, j = 0, ndelegs = 0; i < dst->ndelegs || j < src->ndelegs; ndelegs++) {
		struct tcap_sched_info *n, t;

		/* Let the branch prediction nightmare begin... */
		if (i == dst->ndelegs) {
			n = &src->delegations[j++];
		} else if (j == src->ndelegs || dst->delegations[i].tcap_uid < src->delegations[j].tcap_uid) {
			n = &dst->delegations[i++];
		} else if (dst->delegations[i].tcap_uid > src->delegations[j].tcap_uid) {
			n = &src->delegations[j++];
		} else { /* same scheduler */
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

	/*
	 * If the component is not already a listed root, add it.
	 * Otherwise add it to the front of the list (the current tcap
	 * has permissions to execute now, so that should be
	 * transitively granted to this scheduler.
	 */
	if (__tcap_budget_xfer(dst, src, cycles)) return -EINVAL;
	memcpy(dst->delegations, deleg_tmp, sizeof(struct tcap_sched_info) * ndelegs);
	/* can't get to this point by delegating to yourself, thus 2 schedulers must be involved */
	assert(ndelegs >= 2);
	dst->ndelegs = ndelegs;
	assert(si != -1);
	dst->curr_sched_off        = si;
	dst->perm_prio             = prio;
	tcap_sched_info(dst)->prio = prio;
	/*
	 * TODO: Logic to differentiate between scheduler and non-scheduler tcaps!
	 *       non-scheduler tcaps to have curr_sched_off set to their schedulers and no dedicated uids.
	 */

	// TODO: Add root tcap logic.
	return 0;
}

int
tcap_merge(struct tcap *dst, struct tcap *rm)
{
	if (dst == rm) return -1;
	/* Don't delegate till you we know that we can delete */
	if (tcap_ref(rm) > 1) return -1;
	if (tcap_delegate(dst, rm, 0, tcap_sched_info(dst)->prio)) return -1;
	if (tcap_delete(rm)) assert(0);

	return 0;
}

int
tcap_wakeup(struct tcap *tc, tcap_prio_t prio, tcap_res_t budget, struct thread *thd, struct cos_cpu_local_info *cli)
{
	int                  ret;
	struct next_thdinfo *nti     = &cli->next_ti;
	tcap_prio_t          tmpprio = tcap_sched_info(tc)->prio;

	if (!nti->tc) {
		assert(!nti->thd);
		goto fixup;
	}

	if (tc == nti->tc && prio >= nti->prio) goto fixup;

	tcap_setprio(tc, prio);
	ret = tcap_higher_prio(tc, nti->tc);
	tcap_setprio(tc, tmpprio);
	if (!ret) return 0;

fixup:
	thd_next_thdinfo_update(cli, thd, tc, prio, budget);
	return 0;
}

void
tcap_active_init(struct cos_cpu_local_info *cli)
{
	list_head_init(&cli->tcaps);
}
