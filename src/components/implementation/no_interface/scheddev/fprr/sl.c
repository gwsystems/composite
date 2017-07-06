/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2017, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#include <ps.h>
#include <sl.h>
#include <sl_mod_policy.h>
#include <cos_debug.h>
#include <cos_kernel_api.h>

struct sl_global sl_global_data;

/*
 * These functions are removed from the inlined fast-paths of the
 * critical section (cs) code to save on code size/locality
 */
int
sl_cs_enter_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, thdcap_t curr, sched_tok_t tok)
{
	struct sl_thd *t           = sl_thd_curr();
	struct sl_global *g        = sl__globals();
	int ret;

	/* recursive locks are not allowed */
	assert(csi->s.owner != t->thdcap);
	if (!csi->s.contention) {
		csi->s.contention = 1;
		if (!ps_cas(&g->lock.u.v, cached->v, csi->v)) return 1;
	}
	/* Switch to the owner of the critical section, with inheritance using our tcap/priority */
	if ((ret = cos_defswitch(csi->s.owner, t->prio, g->timeout_next, tok))) return ret;
	/* if we have an outdated token, then we want to use the same repeat loop, so return to that */

	return 1;
}

/* Return 1 if we need a retry, 0 otherwise */
int
sl_cs_exit_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, sched_tok_t tok)
{
	struct sl_thd    *t        = sl_thd_curr();
	struct sl_global *g        = sl__globals();

	if (!ps_cas(&g->lock.u.v, cached->v, 0)) return 1;
	/* let the scheduler thread decide which thread to run next, inheriting our budget/priority */
	cos_defswitch(g->sched_thdcap, t->prio, g->timeout_next, tok);

	return 0;
}

void
sl_thd_block(thdid_t tid)
{
	struct sl_thd *t;

	/* TODO: dependencies not yet supported */
	assert(!tid);

	sl_cs_enter();
	t = sl_thd_curr();
	if (unlikely(t->state == SL_THD_WOKEN)) {
		t->state = SL_THD_RUNNABLE;
		sl_cs_exit();
		return;
	}

	assert(t->state == SL_THD_RUNNABLE);
	t->state = SL_THD_BLOCKED;
	sl_mod_block(sl_mod_thd_policy_get(t));
	sl_cs_exit_schedule();

	return;
}

/*
 * @return: 1 if it's already RUNNABLE.
 *          0 if it was woken up in this call
 */
int
sl_thd_wakeup_no_cs(struct sl_thd *t)
{
	assert(t);

	if (unlikely(t->state == SL_THD_RUNNABLE)) {
		t->state = SL_THD_WOKEN;
		return 1;
	}

	/* TODO: for AEP threads, wakeup events from kernel could be level-triggered. */
	assert(t->state == SL_THD_BLOCKED);
	t->state = SL_THD_RUNNABLE;
	sl_mod_wakeup(sl_mod_thd_policy_get(t));

	return 0;
}

void
sl_thd_wakeup(thdid_t tid)
{
	struct sl_thd *t;
	tcap_t         tcap;
	tcap_prio_t    prio;

	sl_cs_enter();
	t = sl_thd_lkup(tid);
	if (unlikely(!t) || sl_thd_wakeup_no_cs(t)) goto done;

	sl_cs_exit_schedule();

	return;
done:
	sl_cs_exit();
	return;
}

void
sl_thd_yield(thdid_t tid)
{
	struct sl_thd *t = sl_thd_curr();

	sl_cs_enter();
	if (tid) {
		struct sl_thd *to = sl_thd_lkup(tid);

		assert(to);
		sl_cs_exit_switchto(to);
	} else {
		sl_mod_yield(sl_mod_thd_policy_get(t), NULL);
		sl_cs_exit_schedule();
	}

	return;
}

static struct sl_thd *
sl_thd_alloc_init(thdid_t tid, thdcap_t thdcap)
{
	struct sl_thd_policy *tp = NULL;
	struct sl_thd        *t  = NULL;

	tp            = sl_thd_alloc_backend(tid);
	if (!tp) goto done;
	t             = sl_mod_thd_get(tp);

	t->thdid      = tid;
	t->thdcap     = thdcap;
	t->state      = SL_THD_RUNNABLE;
	sl_thd_index_add_backend(sl_mod_thd_policy_get(t));

	t->period     = t->wakeup_cycs = 0;
	t->wakeup_idx = -1;
	t->prio       = TCAP_PRIO_MIN;

done:
	return t;
}

/* boot_thd = 1 if you want to create a boot-up thread in a separate component */
static struct sl_thd *
sl_thd_alloc_intern(cos_thd_fn_t fn, void *data, struct cos_defcompinfo *comp, int boot_thd)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	thdcap_t thdcap;
	thdid_t  tid;

	if (!boot_thd) thdcap = cos_thd_alloc(ci, ci->comp_cap, fn, data);
	else           thdcap = cos_initthd_alloc(ci, comp->ci.comp_cap);
	if (!thdcap) goto done;

	tid = cos_introspect(ci, thdcap, THD_GET_TID);
	assert(tid);
	t = sl_thd_alloc_init(tid, thdcap);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));
done:
	return t;
}

struct sl_thd *
sl_thd_alloc(cos_thd_fn_t fn, void *data)
{ return sl_thd_alloc_intern(fn, data, NULL, 0); }

/* Allocate a thread that executes in the specified component */
struct sl_thd *
sl_thd_comp_alloc(struct cos_defcompinfo *comp)
{ return sl_thd_alloc_intern(NULL, NULL, comp, 1); }

void
sl_thd_free(struct sl_thd *t)
{
	sl_thd_index_rem_backend(sl_mod_thd_policy_get(t));
	t->state = SL_THD_FREE;
	/* TODO: add logic for the graveyard to delay this deallocation if t == current */
	sl_thd_free_backend(sl_mod_thd_policy_get(t));
}

void
sl_thd_param_set(struct sl_thd *t, sched_param_t sp)
{
	sched_param_type_t type;
	unsigned int       value;

	sched_param_get(sp, &type, &value);
	sl_mod_thd_param_set(sl_mod_thd_policy_get(t), type, value);
}

void
sl_timeout_period(microsec_t period)
{
	cycles_t p = sl_usec2cyc(period);

	sl__globals()->period = p;
	sl_timeout_relative(p);
}

/* engage space heater mode */
void
sl_idle(void *d)
{ while (1) ; }

void
sl_init(void)
{
	struct sl_global       *g  = sl__globals();
	struct cos_defcompinfo *ci = cos_defcompinfo_curr_get();

	/* must fit in a word */
	assert(sizeof(struct sl_cs) <= sizeof(unsigned long));

	g->cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	g->lock.u.v     = 0;

	sl_thd_init_backend();
	sl_mod_init();
	sl_timeout_mod_init();

	/* Create the scheduler thread for us */
	g->sched_thd    = sl_thd_alloc_init(cos_thdid(), BOOT_CAPTBL_SELF_INITTHD_BASE);
	assert(g->sched_thd);
	g->sched_thdcap = BOOT_CAPTBL_SELF_INITTHD_BASE;

	g->idle_thd     = sl_thd_alloc(sl_idle, NULL);
	assert(g->idle_thd);

	return;
}

void
sl_sched_loop(void)
{
	while (1) {
		int pending;

		do {
			thdid_t        tid;
			int            blocked, rcvd;
			cycles_t       cycles;
			struct sl_thd *t;

			/*
			 * a child scheduler may receive both scheduling notifications (block/unblock
			 * states of it's child threads) and normal notifications (mainly activations from
			 * it's parent scheduler).
			 */
			pending = cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, RCV_ALL_PENDING,
						&rcvd, &tid, &blocked, &cycles);
			if (!tid) continue;

			t = sl_thd_lkup(tid);
			assert(t);
			/* don't report the idle thread */
			if (unlikely(t == sl__globals()->idle_thd)) continue;

			/*
			 * receiving scheduler notifications is not in critical section mainly for 
			 * 1. scheduler thread can often be blocked in rcv, which can add to 
			 *    interrupt execution or even AEP thread execution overheads.
			 * 2. scheduler events are not acting on the sl_thd or the policy structures, so
			 *    having finer grained locks around the code that modifies sl_thd states is better.
			 */
			if (sl_cs_enter_sched()) continue;
			sl_mod_execution(sl_mod_thd_policy_get(t), cycles);
			if (blocked) sl_mod_block(sl_mod_thd_policy_get(t));
			else         sl_mod_wakeup(sl_mod_thd_policy_get(t));

			sl_cs_exit();
		} while (pending);

		if (sl_cs_enter_sched()) continue;
		/* If switch returns an inconsistency, we retry anyway */
		sl_cs_exit_schedule_nospin();
	}
}
