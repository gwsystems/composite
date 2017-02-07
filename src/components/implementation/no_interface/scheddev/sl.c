/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2017, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#include <ps.h>
#include <sl.h>
#include <sl_policy.h>
#include <assert.h>
#include <cos_kernel_api.h>

/* Default implementations of backend functions */
__attribute__((weak)) struct sl_thd *
sl_thd_alloc_backend(int comp_boot, struct cos_defcompinfo *ci, cos_thd_fn_t fn, void *data)
{
	assert(0);

	return NULL;
}

__attribute__((weak)) void
sl_thd_free_backend(struct sl_thd *t)
{ assert(0); }

__attribute__((weak)) void
sl_thd_index_add_backend(struct sl_thd *t)
{ assert(0); }

__attribute__((weak)) void
sl_thd_index_rem_backend(struct sl_thd *t)
{ assert(0); }

__attribute__((weak)) struct sl_thd *
sl_thd_lookup_backend(thdid_t tid)
{
	assert(0);

	return NULL;
}

struct sl_global sl_global_data;

/*
 * These functions are removed from the inlined fast-paths of the
 * critical section (cs) code to save on code size/locality
 */
void
sl_cs_enter_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, thdcap_t curr, sched_tok_t tok)
{
	struct sl_thd *t = sl_thd_curr();
	struct sl_global *g = sl__globals();

	/* recursive locks are not allowed */
	assert(csi->s.owner != t->thdcap);
	if (!csi->s.contention) {
		csi->s.contention = 1;
		if (!ps_cas(&g->lock.u.v, cached->v, csi->v)) return;
	}
	/* Switch to the owner of the critical section, with inheritance using our tcap/priority */
	cos_switch(csi->s.owner, t->tcap, t->prio, g->timer_next, g->rcv, tok);
	/* if we have an outdated token, then we want to use the same repeat loop, so return to that */
}

/* Return 1 if we need a retry, 0 otherwise */
int
sl_cs_exit_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, sched_tok_t tok)
{
	struct sl_thd    *t = sl_thd_curr();
	struct sl_global *g = sl__globals();

	if (!ps_cas(&g->lock.u.v, cached->v, 0)) return 1;
	/* let the scheduler thread decide which thread to run next, inheriting our budget/priority */
	cos_switch(g->sched_thd, g->curr->tcap, g->curr->prio, g->timer_next, g->rcv, tok);

	return 0;
}

void
sl_thd_block(thdid_t tid)
{
	struct sl_thd *t;

	/* TODO: dependencies not yet supported */
	assert(!tid);

	sl_cs_enter();
	t = sl__globals()->curr;
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

void
sl_thd_wakeup(thdid_t tid)
{
	struct sl_thd *t;
	thdcap_t       thdcap;
	tcap_t         tcap;
	tcap_prio_t    prio;

	sl_cs_enter();
	t = sl_thd_lkup(tid);
	if (unlikely(!t)) goto done;

	if (unlikely(t->state == SL_THD_RUNNABLE)) {
		t->state = SL_THD_WOKEN;
		goto done;
	}

	assert(t->state = SL_THD_BLOCKED);
	t->state = SL_THD_RUNNABLE;
	sl_mod_wakeup(sl_mod_thd_policy_get(t));
	sl_cs_exit_schedule();

	return;
done:
	sl_cs_exit();
	return;
}

/* boot_thd = 1 if you want to create a boot-up thread in a separate component */
static struct sl_thd *
sl_thd_alloc_intern(cos_thd_fn_t fn, void *data, struct cos_defcompinfo *comp, int boot_thd)
{
	struct cos_defcompinfo *ci = cos_defcompinfo_curr_get();
	struct sl_thd          *t  = NULL;

	t = sl_thd_alloc_backend(boot_thd, comp, fn, data);
	if (!t) goto done;

	t->tcap  = ci->sched_aep.tc;
	t->state = SL_THD_RUNNABLE;
	sl_thd_index_add_backend(t);
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
	sl_thd_index_rem_backend(t);
	t->state = SL_THD_FREE;
	/* TODO: add logic for the graveyard to delay this deallocation if t == current */
	sl_thd_free_backend(t);
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
	struct sl_global *g = sl__globals();

	/* must fit in a word */
	assert(sizeof(struct sl_cs) <= sizeof(unsigned long));

	g->cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	sl_mod_init();
	sl_timeout_mod_init();

	g->idle_thd = sl_thd_alloc(sl_idle, NULL);
	assert(g->idle_thd);

	return;
}

void
sl_sched_loop(void)
{
	thdid_t  tid;
	int      blocked;
	cycles_t cycles;

	while (1) {
		struct sl_thd *t;
		sched_tok_t    tok;

		sl_cs_enter();

		while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &blocked, &cycles)) {
			struct sl_thd *t = sl_thd_lkup(tid);

			assert(t);
			/* don't report the idle thread */
			if (unlikely(t == sl__globals()->idle_thd)) continue;
			sl_mod_execution(sl_mod_thd_policy_get(t), cycles);
			if (blocked) sl_mod_block(sl_mod_thd_policy_get(t));
			else         sl_mod_wakeup(sl_mod_thd_policy_get(t));
		}

		/* If switch returns an inconsistency, we retry anyway */
		sl_cs_exit_schedule_nospin();
	}
}
