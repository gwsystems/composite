/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2017, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#include <ps.h>
#include <sl.h>
#include <sl_xcpu.h>
#include <sl_child.h>
#include <sl_mod_policy.h>
#include <cos_debug.h>
#include <cos_kernel_api.h>
#include <bitmap.h>
#include <capmgr.h>

struct sl_global sl_global_data;
struct sl_global_cpu sl_global_cpu_data[NUM_CPU] CACHE_ALIGNED;
static void sl_sched_loop_intern(int non_block) __attribute__((noreturn));
extern struct sl_thd *sl_thd_alloc_init(struct cos_aep_info *aep, asndcap_t sndcap, sl_thd_property_t prps);
extern int sl_xcpu_process_no_cs(void);
extern void sl_xcpu_asnd_alloc(void);

/*
 * These functions are removed from the inlined fast-paths of the
 * critical section (cs) code to save on code size/locality
 */
int
sl_cs_enter_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, thdcap_t curr, sched_tok_t tok)
{
	struct sl_thd        *t = sl_thd_curr();
	struct sl_global_cpu *g = sl__globals_cpu();
	int ret;

	/* recursive locks are not allowed */
	assert(csi->s.owner != sl_thd_thdcap(t));
	if (!csi->s.contention) {
		csi->s.contention = 1;
		if (!ps_cas(&g->lock.u.v, cached->v, csi->v)) return 1;
	}
	/* Switch to the owner of the critical section, with inheritance using our tcap/priority */
	if ((ret = cos_defswitch(csi->s.owner, t->prio, csi->s.owner == sl_thd_thdcap(g->sched_thd) ?
				 TCAP_TIME_NIL : g->timeout_next, tok))) return ret;
	/* if we have an outdated token, then we want to use the same repeat loop, so return to that */

	return 1;
}

/* Return 1 if we need a retry, 0 otherwise */
int
sl_cs_exit_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, sched_tok_t tok)
{
	struct sl_thd        *t = sl_thd_curr();
	struct sl_global_cpu *g = sl__globals_cpu();

	if (!ps_cas(&g->lock.u.v, cached->v, 0)) return 1;
	/* let the scheduler thread decide which thread to run next, inheriting our budget/priority */
	cos_defswitch(g->sched_thdcap, t->prio, TCAP_TIME_NIL, tok);

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

static struct timeout_heap timeout_heap[NUM_CPU] CACHE_ALIGNED;

struct heap *
sl_timeout_heap(void)
{ return &timeout_heap[cos_cpuid()].h; }

static inline void
sl_timeout_block(struct sl_thd *t, cycles_t timeout)
{
	assert(t && t->timeout_idx == -1);
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

void
sl_thd_free_no_cs(struct sl_thd *t)
{
        struct sl_thd *ct = sl_thd_curr();

        assert(t);
        assert(t->state != SL_THD_FREE);
        if (t->state == SL_THD_BLOCKED_TIMEOUT) sl_timeout_remove(t);
        sl_thd_index_rem_backend(sl_mod_thd_policy_get(t));
        sl_mod_thd_delete(sl_mod_thd_policy_get(t));
        t->state = SL_THD_FREE;
        /* TODO: add logic for the graveyard to delay this deallocation if t == current */
        sl_thd_free_backend(sl_mod_thd_policy_get(t));

        /* thread should not continue to run if it deletes itself. */
        if (unlikely(t == ct)) {
                while (1) sl_cs_exit_schedule();
                /* FIXME: should never get here, but tcap mechanism can let a child scheduler run! */
        }
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
sl_timeout_init(microsec_t period)
{
	assert(period >= SL_MIN_PERIOD_US);

	sl_timeout_period(period);
	memset(&timeout_heap[cos_cpuid()], 0, sizeof(struct timeout_heap));
	heap_init(sl_timeout_heap(), SL_MAX_NUM_THDS, __sl_timeout_compare_min, __sl_timeout_update_idx);
}

/*
 * This API is only used by the scheduling thread to block an AEP thread.
 * AEP thread scheduling events could be redundant.
 *
 * @return: 0 if it successfully blocked in this call.
 */
int
sl_thd_sched_block_no_cs(struct sl_thd *t, sl_thd_state_t block_type, cycles_t timeout)
{
	assert(t);
	assert(t != sl__globals_cpu()->idle_thd && t != sl__globals_cpu()->sched_thd);
	assert(block_type == SL_THD_BLOCKED_TIMEOUT || block_type == SL_THD_BLOCKED);

	if (t->schedthd) return 0;
	/*
	 * If an AEP/a child COMP was blocked and an interrupt caused it to wakeup and run
	 * but blocks itself before the scheduler could see the wakeup event.. Scheduler
	 * will only see a BLOCKED event from the kernel.
	 * Only update the timeout if it already exists in the TIMEOUT QUEUE.
	 */
	if (unlikely(t->state == SL_THD_BLOCKED_TIMEOUT || t->state == SL_THD_BLOCKED)) {
		if (t->state == SL_THD_BLOCKED_TIMEOUT) sl_timeout_remove(t);
		goto update;
	}

	assert(sl_thd_is_runnable(t));
	sl_mod_block(sl_mod_thd_policy_get(t));

update:
	t->state = block_type;
	if (block_type == SL_THD_BLOCKED_TIMEOUT) sl_timeout_block(t, timeout);
	t->rcv_suspended = 1;

	return 0;
}

/*
 * Wake "t" up if it was previously blocked on cos_rcv and got
 * to run before the scheduler (tcap-activated)!
 */
static inline int
sl_thd_sched_unblock_no_cs(struct sl_thd *t)
{
	if (unlikely(!t->rcv_suspended)) return 0;
	t->rcv_suspended = 0;
	if (unlikely(t->state != SL_THD_BLOCKED && t->state != SL_THD_BLOCKED_TIMEOUT)) return 0;

	if (likely(t->state == SL_THD_BLOCKED_TIMEOUT)) sl_timeout_remove(t);
	/* make it RUNNABLE */
	sl_thd_wakeup_no_cs_rm(t);

	return 1;
}

/*
 * @return: 1 if it's already WOKEN.
 *	    0 if it successfully blocked in this call.
 */
int
sl_thd_block_no_cs(struct sl_thd *t, sl_thd_state_t block_type, cycles_t timeout)
{
	assert(t);
	assert(t != sl__globals_cpu()->idle_thd && t != sl__globals_cpu()->sched_thd);
	assert(sl_thd_curr() == t); /* only current thread is allowed to block itself */
	assert(block_type == SL_THD_BLOCKED_TIMEOUT || block_type == SL_THD_BLOCKED);

	if (t->schedthd) {
		sl_parent_notif_block_no_cs(t->schedthd, t);

		return 0;
	}

	if (unlikely(t->state == SL_THD_WOKEN)) {
		assert(!t->rcv_suspended);
		t->state = SL_THD_RUNNABLE;
		return 1;
	}

	/* reset rcv_suspended if the scheduler thinks "curr" was suspended on cos_rcv previously */
	sl_thd_sched_unblock_no_cs(t);
	assert(t->state == SL_THD_RUNNABLE);
	sl_mod_block(sl_mod_thd_policy_get(t));
	t->state = block_type;
	if (block_type == SL_THD_BLOCKED_TIMEOUT) sl_timeout_block(t, timeout);

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
	if (sl_thd_block_no_cs(t, SL_THD_BLOCKED, 0)) {
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
	if (sl_thd_block_no_cs(t, SL_THD_BLOCKED_TIMEOUT, timeout)) {
		sl_cs_exit();
		return 1;
	}
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

	assert(t->period);
	if (sl_thd_block_timeout_intern(tid, 0)) goto done;
	wcycs = t->wakeup_cycs;
	pcycs = t->periodic_cycs;
	if (wcycs > pcycs) jitter = ((unsigned int)((wcycs - pcycs) / t->period)) + 1;

done:
	return jitter;
}

void
sl_thd_block_expiry(struct sl_thd *t)
{
	cycles_t abs_timeout = 0;

	assert(t != sl__globals_cpu()->idle_thd && t != sl__globals_cpu()->sched_thd);
	sl_cs_enter();
	if (!(t->properties & SL_THD_PROPERTY_OWN_TCAP)) {
		assert(!t->rcv_suspended);
		abs_timeout = sl__globals_cpu()->timeout_next;
	} else {
		assert(t->period);
		abs_timeout = t->last_replenish + t->period;
	}

	/* reset rcv_suspended if the scheduler thinks "t" was suspended on cos_rcv previously */
	sl_thd_sched_unblock_no_cs(t);
	sl_thd_sched_block_no_cs(t, SL_THD_BLOCKED_TIMEOUT, abs_timeout);

	sl_cs_exit();
}

/*
 * This API is only used by the scheduling thread to wakeup an AEP thread.
 * AEP thread scheduling events could be redundant.
 *
 * @return: 1 if it's already WOKEN or RUNNABLE.
 *	    0 if it successfully blocked in this call.
 */
int
sl_thd_sched_wakeup_no_cs(struct sl_thd *t)
{
	assert(t);

	if (unlikely(!t->rcv_suspended)) return 1; /* not blocked on cos_rcv, so don't mess with user-level thread states */
	t->rcv_suspended = 0;
	/*
	 * If a thread was preempted and scheduler updated it to RUNNABLE status and if that AEP
	 * was activated again (perhaps by tcap preemption logic) and expired it's budget, it could
	 * result in the scheduler having a redundant WAKEUP event.
	 *
	 * Thread could be in WOKEN state:
	 * Perhaps the thread was blocked waiting for a lock and was woken up by another thread and
	 * and then scheduler sees some redundant wakeup event through "asnd" or "tcap budget expiry".
	 */
	if (unlikely(t->state == SL_THD_RUNNABLE || t->state == SL_THD_WOKEN)) return 1;

	assert(t->state == SL_THD_BLOCKED || t->state == SL_THD_BLOCKED_TIMEOUT);
	if (t->state == SL_THD_BLOCKED_TIMEOUT) sl_timeout_remove(t);
	t->state = SL_THD_RUNNABLE;
	sl_mod_wakeup(sl_mod_thd_policy_get(t));

	return 0;
}

/*
 * @return: 1 if it's already RUNNABLE.
 *          0 if it was woken up in this call
 */
int
sl_thd_wakeup_no_cs_rm(struct sl_thd *t)
{
	assert(t);
	assert(t != sl__globals_cpu()->idle_thd && t != sl__globals_cpu()->sched_thd);

	assert(t->state == SL_THD_BLOCKED || t->state == SL_THD_BLOCKED_TIMEOUT);
	t->state = SL_THD_RUNNABLE;
	sl_mod_wakeup(sl_mod_thd_policy_get(t));
	t->rcv_suspended = 0;

	return 0;
}

int
sl_thd_wakeup_no_cs(struct sl_thd *t)
{
	assert(t);
	assert(sl_thd_curr() != t); /* current thread is not allowed to wake itself up */

	if (t->schedthd) {
		sl_parent_notif_wakeup_no_cs(t->schedthd, t);

		return 0;
	}

	if (unlikely(sl_thd_is_runnable(t))) {
		/* t->state == SL_THD_WOKEN? multiple wakeups? */
		t->state = SL_THD_WOKEN;
		return 1;
	}

	assert(t->state == SL_THD_BLOCKED || t->state == SL_THD_BLOCKED_TIMEOUT);
	if (t->state == SL_THD_BLOCKED_TIMEOUT) sl_timeout_remove(t);
	return sl_thd_wakeup_no_cs_rm(t);
}

void
sl_thd_wakeup(thdid_t tid)
{
	struct sl_thd *t;

	sl_cs_enter();
	t = sl_thd_lkup(tid);
	if (unlikely(!t)) goto done;

	if (sl_thd_wakeup_no_cs(t)) goto done;
	sl_cs_exit_schedule();

	return;
done:
	sl_cs_exit();
	return;
}

CWEAKSYMB void
capmgr_set_tls(thdcap_t cap, void* tls_addr)
{
	/* TODO: implement this when some tests depend on lib sl without capmgr */
	assert(0);
}

void sched_set_tls(void* tls_addr)
{
	struct sl_thd *t = sl_thd_curr();

	thdcap_t thdcap = sl_thd_thdcap(t);

	capmgr_set_tls(thdcap, tls_addr);
}

void
sl_thd_yield_cs_exit(thdid_t tid)
{
	struct sl_thd *t = sl_thd_curr();

	/* reset rcv_suspended if the scheduler thinks "curr" was suspended on cos_rcv previously */
	sl_thd_sched_unblock_no_cs(t);
	if (tid) {
		struct sl_thd *to = sl_thd_lkup(tid);

		assert(to);
		sl_cs_exit_switchto(to);
	} else {
		if (likely(t != sl__globals_cpu()->sched_thd && t != sl__globals_cpu()->idle_thd)) sl_mod_yield(sl_mod_thd_policy_get(t), NULL);
		sl_cs_exit_schedule();
	}
}

void
sl_thd_yield(thdid_t tid)
{
	sl_cs_enter();
	sl_thd_yield_cs_exit(tid);
}

void
sl_thd_event_info_reset(struct sl_thd *t)
{
	t->event_info.blocked = 0;
	t->event_info.cycles  = 0;
	t->event_info.timeout = 0;
}

static inline void
sl_thd_event_enqueue(struct sl_thd *t, int blocked, cycles_t cycles, tcap_time_t timeout)
{
	struct sl_global_cpu *g = sl__globals_cpu();

	if (ps_list_singleton(t, SL_THD_EVENT_LIST)) ps_list_head_append(&g->event_head, t, SL_THD_EVENT_LIST);

	t->event_info.blocked  = blocked;
	t->event_info.cycles  += cycles;
	t->event_info.timeout  = timeout;
}

static inline void
sl_thd_event_dequeue(struct sl_thd *t, int *blocked, cycles_t *cycles, tcap_time_t *timeout)
{
	ps_list_rem(t, SL_THD_EVENT_LIST);

	*blocked = t->event_info.blocked;
	*cycles  = t->event_info.cycles;
	*timeout = t->event_info.timeout;
	sl_thd_event_info_reset(t);
}

void
sl_thd_exit()
{
	sl_thd_free(sl_thd_curr());
}

void
sl_thd_param_set(struct sl_thd *t, sched_param_t sp)
{
	sched_param_type_t type;
	unsigned int       value;

	assert(t);

	sched_param_get(sp, &type, &value);

	sl_cs_enter();
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
	sl_cs_exit();
}

void
sl_timeout_period(microsec_t period)
{
	cycles_t p = sl_usec2cyc(period);

	sl__globals_cpu()->period = p;
	sl_timeout_relative(p);
}

/* engage space heater mode */
void
sl_idle(void *d)
{ while (1) ; }

/* call from the user? */
static void
sl_global_init(u32_t *cpu_bmp)
{
	struct sl_global *g = sl__globals();
	unsigned int i = 0;

	memset(g, 0, sizeof(struct sl_global));

	for (i = 0; i < NUM_CPU; i++) {
		if (!bitmap_check(cpu_bmp, i)) continue;

		bitmap_set(g->cpu_bmp, i);
		ck_ring_init(sl__ring(i), SL_XCPU_RING_SIZE);
	}
}

void
sl_init_cpubmp(microsec_t period, u32_t *cpubmp)
{
	int i;
	static volatile long long int first    = 1, init_done = 0;
	struct cos_defcompinfo *dci  = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci   = cos_compinfo_get(dci);
	struct sl_global_cpu   *g    = sl__globals_cpu();
	struct cos_aep_info    *saep = cos_sched_aep_get(dci);

	if (ps_cas((unsigned long *)&first, 1, 0)) {
		sl_global_init(cpubmp);

		ps_faa((unsigned long *)&init_done, 1);
	} else {
		/* wait until global ring buffers are initialized correctly! */
		while (!ps_load(&init_done)) ;
		/* make sure this scheduler is active on this cpu/core */
		assert(sl_cpu_active());
	}

	/* must fit in a word */
	assert(sizeof(struct sl_cs) <= sizeof(unsigned long));
	memset(g, 0, sizeof(struct sl_global_cpu));

	g->cyc_per_usec    = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	g->lock.u.v        = 0;

	sl_thd_init_backend();
	sl_mod_init();
	sl_timeout_init(period);

	/* Create the scheduler thread for us. cos_sched_aep_get() is from global(static) memory */
	g->sched_thd       = sl_thd_alloc_init(saep, 0, 0);
	assert(g->sched_thd);
	g->sched_thdcap    = saep->thd;
	g->sched_tcap      = saep->tc;
	g->sched_rcv       = saep->rcv;
	assert(g->sched_rcv);
	g->sched_thd->prio = TCAP_PRIO_MAX;
	ps_list_head_init(&g->event_head);

	g->idle_thd        = sl_thd_alloc(sl_idle, NULL);
	assert(g->idle_thd);

	/* all cores that this sched runs on, must be initialized by now so "asnd"s can be created! */
	sl_xcpu_asnd_alloc();

	return;
}


void
sl_init(microsec_t period)
{
	u32_t cpubmp[NUM_CPU_BMP_WORDS] = { 0 };

	/* runs on all cores.. */
	bitmap_set_contig(cpubmp, 0, NUM_CPU, 1);
	sl_init_cpubmp(period, cpubmp);
}

static void
sl_sched_loop_intern(int non_block)
{
	struct sl_global_cpu *g   = sl__globals_cpu();
	rcv_flags_t           rfl = (non_block ? RCV_NON_BLOCKING : 0) | RCV_ALL_PENDING;

	assert(sl_cpu_active());

	while (1) {
		int pending;

		do {
			thdid_t        tid;
			int            blocked, rcvd;
			cycles_t       cycles;
			tcap_time_t    timeout = g->timeout_next, thd_timeout;
			struct sl_thd *t = NULL, *tn = NULL;
			struct sl_child_notification notif;

			/*
			 * a child scheduler may receive both scheduling notifications (block/unblock
			 * states of it's child threads) and normal notifications (mainly activations from
			 * it's parent scheduler).
			 */
			pending = cos_sched_rcv(g->sched_rcv, rfl, timeout,
						&rcvd, &tid, &blocked, &cycles, &thd_timeout);
			if (!tid) goto pending_events;

			t = sl_thd_lkup(tid);
			assert(t);
			/* don't report the idle thread or a freed thread */
			if (unlikely(t == g->idle_thd || t->state == SL_THD_FREE)) goto pending_events;

			/*
			 * Failure to take the CS because another thread is holding it and switching to
			 * that thread cannot succeed because scheduler has pending events causes the event
			 * just received to be dropped.
			 * To avoid dropping events, add the events to the scheduler event list and processing all
			 * the pending events after the scheduler can successfully take the lock.
			 */
			sl_thd_event_enqueue(t, blocked, cycles, thd_timeout);

pending_events:
			if (ps_list_head_empty(&g->event_head) &&
			    ck_ring_size(sl__ring_curr()) == 0 &&
			    sl_child_notif_empty()) continue;

			/*
			 * receiving scheduler notifications is not in critical section mainly for
			 * 1. scheduler thread can often be blocked in rcv, which can add to
			 *    interrupt execution or even AEP thread execution overheads.
			 * 2. scheduler events are not acting on the sl_thd or the policy structures, so
			 *    having finer grained locks around the code that modifies sl_thd states is better.
			 */
			if (sl_cs_enter_sched()) continue;

			ps_list_foreach_del(&g->event_head, t, tn, SL_THD_EVENT_LIST) {
				/* remove the event from the list and get event info */
				sl_thd_event_dequeue(t, &blocked, &cycles, &thd_timeout);

				/* outdated event for a freed thread */
				if (t->state == SL_THD_FREE) continue;

				sl_mod_execution(sl_mod_thd_policy_get(t), cycles);

				if (blocked) {
					sl_thd_state_t state = SL_THD_BLOCKED;
					cycles_t abs_timeout = 0;

					if (likely(cycles)) {
						if (thd_timeout) {
							state       = SL_THD_BLOCKED_TIMEOUT;
							abs_timeout = tcap_time2cyc(thd_timeout, sl_now());
						}
						sl_thd_sched_block_no_cs(t, state, abs_timeout);
					}
				} else {
					sl_thd_sched_wakeup_no_cs(t);
				}
			}

			/* process notifications from the parent of my threads */
			while (sl_child_notif_dequeue(&notif)) {
				struct sl_thd *t = sl_thd_lkup(notif.tid);

				if (notif.type == SL_CHILD_THD_BLOCK) sl_thd_block_no_cs(t, SL_THD_BLOCKED, 0);
				else                                  sl_thd_wakeup_no_cs(t);
			}

			/* process cross-core requests */
			sl_xcpu_process_no_cs();

			sl_cs_exit();
		} while (pending > 0);

		if (sl_cs_enter_sched()) continue;
		/* If switch returns an inconsistency, we retry anyway */
		sl_cs_exit_schedule_nospin();
	}
}

void
sl_sched_loop(void)
{
	sl_sched_loop_intern(0);
}

void
sl_sched_loop_nonblock(void)
{
	sl_sched_loop_intern(1);
}
