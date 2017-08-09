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
	assert(csi->s.owner != sl_thd_thdcap(t));
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

/* Timeout and wakeup functionality */
/*
 * TODO: 
 * (comments from Gabe)
 * We likely want to replace all of this with rb-tree with nodes internal to the threads.
 * This heap is fast, but the static memory allocation is not great.
 */
struct timeout_heap {
	struct heap  h;
	void        *data[SL_MAX_NUM_THDS];
};

static struct timeout_heap timeout_heap;

struct heap *
sl_timeout_heap(void)
{ return &timeout_heap.h; }

static inline void
sl_timeout_block(struct sl_thd *t, cycles_t timeout)
{
	assert(t && t->timeout_idx == -1); /* not already in heap */
	assert(heap_size(sl_timeout_heap()) < SL_MAX_NUM_THDS);

	if (!timeout) {
		cycles_t tmp = t->periodic_cycs;

		assert(t->period);
		t->periodic_cycs += t->period; /* implicit timeout = task period */
		assert(tmp < t->periodic_cycs); /* wraparound check */
		t->timeout_cycs   = t->periodic_cycs;
	} else {
		t->timeout_cycs   = timeout;
	}

	t->wakeup_cycs = 0;
	heap_add(sl_timeout_heap(), t);
}

static inline void
sl_timeout_remove(struct sl_thd *t)
{
	assert(t && t->timeout_idx > 0);
	assert(heap_size(sl_timeout_heap())); 

	heap_remove(sl_timeout_heap(), t->timeout_idx);
	t->timeout_idx = -1;
}

static int
__sl_timeout_compare_min(void *a, void *b)
{
	/* FIXME: logic for wraparound in either timeout_cycs */
	return ((struct sl_thd *)a)->timeout_cycs <= ((struct sl_thd *)b)->timeout_cycs;
}

static void
__sl_timeout_update_idx(void *e, int pos)
{ ((struct sl_thd *)e)->timeout_idx = pos; }

static void
sl_timeout_init(void)
{
	sl_timeout_period(SL_PERIOD_US);
	heap_init(sl_timeout_heap(), SL_MAX_NUM_THDS, __sl_timeout_compare_min, __sl_timeout_update_idx);
}

/*
 * @return: 1 if it's already WOKEN.
 *	    0 if it successfully blocked in this call.
 */
int
sl_thd_block_no_cs(struct sl_thd *t, sl_thd_state block_type)
{
	assert(t);
	assert(block_type == SL_THD_BLOCKED_TIMEOUT || block_type == SL_THD_BLOCKED);

	if (unlikely(t->state == SL_THD_WOKEN)) {
		t->state = SL_THD_RUNNABLE;
		return 1;
	}

	if (unlikely(t->state == SL_THD_BLOCKED || t->state == SL_THD_BLOCKED_TIMEOUT)) {
		/*
		 * If an AEP/a child COMP was blocked and an interrupt caused it to wakeup and run
		 * but blocks itself before the scheduler could see the wakeup event.. Scheduler
		 * will only see a BLOCKED event from the kernel.
		 */
		return 0;
	}
	assert(t->state == SL_THD_RUNNABLE);
	t->state = block_type;
	sl_mod_block(sl_mod_thd_policy_get(t));

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
	if (sl_thd_block_no_cs(t, SL_THD_BLOCKED)) {
		sl_cs_exit();
		return;
	}
	sl_cs_exit_schedule();

	return;
}

/*
 * if timeout == 0, blocks on timeout = last periodic wakeup + task period
 * @return: 0 if blocked in this call. 1 if already WOKEN!
 */
static inline int 
sl_thd_block_timeout_intern(thdid_t tid, cycles_t timeout)
{
	struct sl_thd *t;

	sl_cs_enter();
	t = sl_thd_curr();

	if (sl_thd_block_no_cs(t, SL_THD_BLOCKED_TIMEOUT)) {
		sl_cs_exit();
		return 1;
	}

	assert(timeout || t->period);
	sl_timeout_block(t, timeout);
	sl_cs_exit_schedule();

	return 0;
}

cycles_t
sl_thd_block_timeout(thdid_t tid, cycles_t abs_timeout)
{
	cycles_t jitter  = 0, wcycs, tcycs;
	struct sl_thd *t = sl_thd_curr();

	/* TODO: dependencies not yet supported */
	assert(!tid);

	if (unlikely(!abs_timeout)) {
		sl_thd_block(tid);
		goto done;
	}

	if (sl_thd_block_timeout_intern(tid, abs_timeout)) goto done;
	wcycs = t->wakeup_cycs;
	tcycs = t->timeout_cycs;
	if (wcycs > tcycs) jitter = wcycs - tcycs;

done:
	return jitter;
}

unsigned int
sl_thd_block_periodic(thdid_t tid)
{
	cycles_t wcycs, pcycs;
	unsigned int jitter = 0;
	struct sl_thd *t    = sl_thd_curr();

	/* TODO: dependencies not yet supported */
	assert(!tid);

	if (sl_thd_block_timeout_intern(tid, 0)) goto done;
	wcycs = t->wakeup_cycs;
	pcycs = t->periodic_cycs;
	if (wcycs > pcycs) jitter = ((unsigned int)((wcycs - pcycs) / t->period)) + 1;

done:
	return jitter;
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
	assert(t->state == SL_THD_BLOCKED || t->state == SL_THD_BLOCKED_TIMEOUT);
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
	if (unlikely(!t)) goto done;

	if (t->state == SL_THD_BLOCKED_TIMEOUT) sl_timeout_remove(t);
	if (sl_thd_wakeup_no_cs(t)) goto done;
	sl_cs_exit_schedule();

	return;
done:
	sl_cs_exit();
	return;
}

void
sl_thd_yield_cs_exit(thdid_t tid)
{
	struct sl_thd *t = sl_thd_curr();

	if (tid) {
		struct sl_thd *to = sl_thd_lkup(tid);

		assert(to);
		sl_cs_exit_switchto(to);
	} else {
		sl_mod_yield(sl_mod_thd_policy_get(t), NULL);
		sl_cs_exit_schedule();
	}
}

void
sl_thd_yield(thdid_t tid)
{
	sl_cs_enter();
	sl_thd_yield_cs_exit(tid);
}

static void
sl_thd_aepinfo_caps_init(struct sl_thd *t, thdcap_t thd, arcvcap_t rcv, tcap_t tc)
{
	assert(t);

	sl_thd_aepinfo(t)->thd = thd;
	sl_thd_aepinfo(t)->rcv = rcv;
	sl_thd_aepinfo(t)->tc  = tc;
}

static struct sl_thd *
sl_thd_alloc_init(thdid_t tid, thdcap_t thdcap, arcvcap_t rcvcap, tcap_t tcap,
		  asndcap_t sndcap, sl_thd_type type)
{
	struct sl_thd_policy *tp = NULL;
	struct sl_thd        *t  = NULL;

	tp = sl_thd_alloc_backend(tid);
	if (!tp) goto done;
	t  = sl_mod_thd_get(tp);
	
	t->thdid          = tid;
	t->type           = type;
	sl_thd_aepinfo_caps_init(t, thdcap, rcvcap, tcap);
	t->sndcap         = sndcap;
	t->state          = SL_THD_RUNNABLE;
	sl_thd_index_add_backend(sl_mod_thd_policy_get(t));

	t->budget         = 0;
	t->last_replenish = 0;
	t->period         = t->timeout_cycs = t->periodic_cycs = 0;
	t->wakeup_cycs    = 0;
	t->timeout_idx    = -1;
	t->prio           = TCAP_PRIO_MIN;

done:
	return t;
}

static struct sl_thd *
sl_thd_alloc_intern(cos_thd_fn_t fn, void *data)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	thdcap_t thdcap;
	thdid_t tid;

	thdcap = cos_thd_alloc(ci, ci->comp_cap, fn, data);
	if (!thdcap) goto done;

	tid = cos_introspect(ci, thdcap, THD_GET_TID);
	assert(tid);
	t = sl_thd_alloc_init(tid, thdcap, 0, 0, 0, SL_THD_THD);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

static struct sl_thd *
sl_thd_aep_alloc_intern(cos_aepthd_fn_t fn, void *data, struct cos_defcompinfo *comp, sl_thd_type type)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;
	struct sl_thd          *t   = NULL;
	asndcap_t               snd = 0;
	struct cos_aep_info     aep;
	thdid_t                 tid;
	int                     ret;
	
	switch(type) {
	case SL_THD_AEP:
	{
		ret = cos_aep_tcap_alloc(&aep, sl_thd_aepinfo(sl__globals()->sched_thd)->tc, fn, data);
		if (ret) goto done;
		break;
	}
	case SL_THD_AEP_TCAP:
	{
		ret = cos_aep_alloc(&aep, fn, data);
		if (ret) goto done;
		break;
	}
	case SL_THD_COMP:
	case SL_THD_COMP_TCAP:
	{
		struct cos_aep_info *sa;

		assert(comp);
		sa = cos_sched_aep_get(comp);

		/* FIXME: inefficient.. find a zerocopy way! */
		aep.tc  = sa->tc;
		aep.thd = sa->thd;
		aep.rcv = sa->rcv;

		snd = cos_asnd_alloc(ci, aep.rcv, ci->captbl_cap);
		assert(snd);
		break;
	}
	default:
	{
		assert(0);
		break;
	}
	}

	tid = cos_introspect(ci, aep.thd, THD_GET_TID);
	assert(tid);
	t = sl_thd_alloc_init(tid, aep.thd, aep.rcv, aep.tc, snd, type);
	sl_mod_thd_create(sl_mod_thd_policy_get(t));

done:
	return t;
}

struct sl_thd *
sl_thd_alloc(cos_thd_fn_t fn, void *data)
{
	struct sl_thd *t;

	sl_cs_enter();
	t = sl_thd_alloc_intern(fn, data);
	sl_cs_exit();

	return t;
}

struct sl_thd *
sl_thd_aep_alloc(cos_aepthd_fn_t fn, void *data, sl_thd_type type)
{
	struct sl_thd *t;

	assert(type == SL_THD_AEP || type == SL_THD_AEP_TCAP);

	sl_cs_enter();
	t = sl_thd_aep_alloc_intern(fn, data, NULL, type);
	sl_cs_exit();

	return t;
}

/* sl object for inithd in the child comp */
struct sl_thd *
sl_thd_comp_init(struct cos_defcompinfo *comp, sl_thd_type type)
{
	struct sl_thd *t;

	assert(comp);
	assert(type == SL_THD_COMP || type == SL_THD_COMP_TCAP);
	if (type == SL_THD_COMP) assert(cos_sched_aep_get(comp)->tc == sl_thd_tcap(sl__globals()->sched_thd));

	sl_cs_enter();
	t = sl_thd_aep_alloc_intern(NULL, NULL, comp, type);
	sl_cs_exit();

	return t;
}

void
sl_thd_free(struct sl_thd *t)
{
	struct sl_thd *ct = sl_thd_curr();

	sl_cs_enter();

	assert(t->state != SL_THD_FREE);
	if (t->state == SL_THD_BLOCKED_TIMEOUT) sl_timeout_remove(t);
	sl_thd_index_rem_backend(sl_mod_thd_policy_get(t));
	sl_mod_thd_delete(sl_mod_thd_policy_get(t));
	t->state = SL_THD_FREE;
	/* TODO: add logic for the graveyard to delay this deallocation if t == current */
	sl_thd_free_backend(sl_mod_thd_policy_get(t));

	/* thread should not continue to run if it deletes itself. */
	if (unlikely(t == ct)) {
		sl_cs_exit_schedule();

		/* should never get here */
		assert(0);
	}
	sl_cs_exit();
}

void
sl_thd_param_set(struct sl_thd *t, sched_param_t sp)
{
	sched_param_type_t type;
	unsigned int       value;

	sched_param_get(sp, &type, &value);

	switch (type) {
	case SCHEDP_WINDOW:
	{
		t->period = sl_usec2cyc(value);
		t->periodic_cycs = sl_now(); /* TODO: synchronize for all tasks */
		break;
	}
	case SCHEDP_BUDGET:
	{
		t->budget = sl_usec2cyc(value);
		break;
	}
	default: break;
	}

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

	g->cyc_per_usec    = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	g->lock.u.v        = 0;

	sl_thd_init_backend();
	sl_mod_init();
	sl_timeout_init();

	/* Create the scheduler thread for us */
	g->sched_thd       = sl_thd_alloc_init(cos_thdid(), BOOT_CAPTBL_SELF_INITTHD_BASE, 
					    BOOT_CAPTBL_SELF_INITRCV_BASE, BOOT_CAPTBL_SELF_INITTCAP_BASE, 0, SL_THD_THD);
	assert(g->sched_thd);
	g->sched_thdcap    = BOOT_CAPTBL_SELF_INITTHD_BASE;
	g->sched_tcap      = BOOT_CAPTBL_SELF_INITTCAP_BASE;
	g->sched_rcv       = BOOT_CAPTBL_SELF_INITRCV_BASE;
	g->sched_thd->prio = 0;

	g->idle_thd        = sl_thd_alloc(sl_idle, NULL);
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
			pending = cos_sched_rcv(sl__globals()->sched_rcv, RCV_ALL_PENDING,
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

			if (blocked) sl_thd_block_no_cs(t, SL_THD_BLOCKED); /* TODO: use absolute timeouts by the child AEP/COMP! */
			else         sl_thd_wakeup_no_cs(t);

			sl_cs_exit();
		} while (pending);

		if (sl_cs_enter_sched()) continue;
		/* If switch returns an inconsistency, we retry anyway */
		sl_cs_exit_schedule_nospin();
	}
}
