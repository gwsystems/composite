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
#include "list.h"

#ifndef TCAP_MAX_DELEGATIONS
#define TCAP_MAX_DELEGATIONS 16
#endif

#define TCAP_TIMER_DIFF (1 << 9)

struct cap_tcap {
	struct cap_header h;
	struct tcap *     tcap;
	cpuid_t           cpuid;
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
	struct thread *    arcv_ep; /* the arcv endpoint this tcap is hooked into */
	u32_t              refcnt;
	struct tcap_budget budget;
	u8_t               ndelegs, curr_sched_off;
	u16_t              cpuid;
	tcap_prio_t        perm_prio;

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
	struct list_node       active_list;
};

void tcap_active_init(struct cos_cpu_local_info *cli);
int  tcap_activate(struct captbl *ct, capid_t cap, capid_t capin, struct tcap *tcap_new);
int  tcap_delegate(struct tcap *tcapdst, struct tcap *tcapsrc, tcap_res_t cycles, tcap_prio_t prio);
int  tcap_merge(struct tcap *dst, struct tcap *rm);
void tcap_promote(struct tcap *t, struct thread *thd);
int
tcap_wakeup(struct tcap *tc, tcap_prio_t prio, tcap_res_t budget, struct thread *thd, struct cos_cpu_local_info *cli);

struct thread *tcap_tick_handler(void);
void           tcap_timer_choose(int c);

static inline struct tcap_sched_info *
tcap_sched_info(struct tcap *t)
{
	return &t->delegations[t->curr_sched_off];
}

static inline void
tcap_ref_take(struct tcap *t)
{
	t->refcnt++;
}

static inline void
tcap_ref_release(struct tcap *t)
{
	t->refcnt--;
}

static inline int
tcap_ref(struct tcap *t)
{
	return t->refcnt;
}

static inline struct thread *
tcap_rcvcap_thd(struct tcap *tc)
{
	return tc->arcv_ep;
}

/*** Active TCap list ***/

static inline int
tcap_is_active(struct tcap *t)
{
	return !list_empty(&t->active_list);
}

static inline void
tcap_active_add_before(struct tcap *existing, struct tcap *t)
{
	list_add_before(&existing->active_list, &t->active_list);
}

static inline void
tcap_active_add_after(struct tcap *existing, struct tcap *t)
{
	list_add_after(&existing->active_list, &t->active_list);
}

static inline struct tcap *
tcap_active_next(struct cos_cpu_local_info *cli)
{
	return (struct tcap *)list_first(&cli->tcaps);
}

static inline void
tcap_active_rem(struct tcap *t)
{
	list_rem(&t->active_list);
}

static unsigned int
tcap_cycles_same(cycles_t a, cycles_t b)
{
	return cycles_same(a, b, (cycles_t)chal_cyc_thresh());
}

/**
 * Expend @cycles amount of budget.
 * Return the amount of budget that is left in the tcap.
 */
static inline tcap_res_t
tcap_consume(struct tcap *t, tcap_res_t cycles)
{
	assert(t);
	if (TCAP_RES_IS_INF(t->budget.cycles)) return 0;
	if (cycles >= t->budget.cycles || tcap_cycles_same(cycles, t->budget.cycles)) {
		t->budget.cycles = 0;
		tcap_active_rem(t); /* no longer active */

		/* "declassify" the time by keeping only the current tcap's priority */
		t->ndelegs = 1;
		if (t->curr_sched_off != 0) {
			memcpy(&t->delegations[0], tcap_sched_info(t), sizeof(struct tcap_sched_info));
			t->curr_sched_off = 0;
		}
	} else {
		t->budget.cycles -= cycles;
	}

	return t->budget.cycles;
}

static inline tcap_res_t
tcap_left(struct tcap *t)
{
	return t->budget.cycles;
}

static inline int
tcap_expended(struct tcap *t)
{
	return tcap_left(t) == 0;
}

static inline void
tcap_setprio(struct tcap *t, tcap_prio_t p)
{
	assert(t);
	tcap_sched_info(t)->prio = p;
}

static inline struct tcap *
tcap_current(struct cos_cpu_local_info *cos_info)
{
	return (struct tcap *)(cos_info->curr_tcap);
}

static inline void
tcap_current_set(struct cos_cpu_local_info *cos_info, struct tcap *t)
{
	struct tcap *curr = tcap_current(cos_info);

	/* remove transient prio on current tcap before switching to a new tcap */
	if (curr->perm_prio != tcap_sched_info(curr)->prio) tcap_setprio(curr, curr->perm_prio);
	cos_info->curr_tcap = t;
}

/* hack to avoid header file recursion */
void __thd_exec_add(struct thread *t, cycles_t cycles);

static inline int
tcap_budgets_update(struct cos_cpu_local_info *cos_info, struct thread *t, struct tcap *next, cycles_t *now)
{
	cycles_t     cycles, expended;
	struct tcap *curr = tcap_current(cos_info);

	cycles = *now    = tsc();
	expended         = cycles - cos_info->cycles;
	cos_info->cycles = cycles;
	__thd_exec_add(t, expended);
	tcap_consume(curr, expended);

	return tcap_expended(curr);
}

/*
 * Update the current tcap's (@next's) cycle count, set the next
 * oneshot @timeout, and return the cycles @expended if they should be
 * tracked.
 */
static inline void
tcap_timer_update(struct cos_cpu_local_info *cos_info, struct tcap *next, tcap_time_t timeout, cycles_t now)
{
	cycles_t   timer, timeout_cyc;
	tcap_res_t left;

	/* next == INF? no timer required. */
	left = tcap_left(next);
	if (timeout == TCAP_TIME_NIL && TCAP_RES_IS_INF(left)) {
		cos_info->next_timer = 0;
		chal_timer_disable();
		return;
	}

	/* timeout based on the tcap budget... */
	timer       = now + left;
	timeout_cyc = tcap_time2cyc(timeout, now);
	/* ...or explicit timeout within the bounds of the budget */
	if (timeout != TCAP_TIME_NIL && timeout_cyc < timer) {
		if (tcap_time_lessthan(timeout, tcap_cyc2time(now)))
			timer = now;
		else
			timer = timeout_cyc;
	}

	if (cycles_same(now, timer, TCAP_TIMER_DIFF)) timer = now + TCAP_TIMER_DIFF;
	if (cycles_same(cos_info->next_timer, timer, TCAP_TIMER_DIFF) && cos_info->next_timer) return;

	assert(timer); /* TODO: wraparound check when timer == 0 */
	cos_info->next_timer = timer;
	chal_timer_set(timer);
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
	int ret = 0;

	if (tcap_expended(a)) return 0;
	if (unlikely(a == c)) return 1;

	for (i = 0, j = 0; i < a->ndelegs && j < c->ndelegs;) {
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
	return ret;
}

static inline int
tcap_introspect(struct tcap *t, unsigned long op, unsigned long *retval)
{
	switch (op) {
	case TCAP_GET_BUDGET:
		*retval = t->budget.cycles;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#endif /* TCAP_H */
