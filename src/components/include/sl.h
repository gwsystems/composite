/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2017, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

/*
 * Scheduler library (sl) abstractions and functions.
 *
 * This library does a few things:
 * 1. hide the esoteric nature of the cos_kernel_api's dispatch
 *    methods, and the scheduler event notifications + scheduler
 *    thread,
 * 2. provide synchronization around scheduler data-strutures, and
 * 3. abstract shared details about thread blocking/wakeup, lookup,
 *    and inter-thread dependency management.
 *
 * This library interacts with a number of other libraries/modules.
 *
 * - uses: dispatching functions in the cos_kernel_api (which uses,
 *   the kernel system call layer)
 * - uses: parsec (ps) for atomic instructions and synchronization
 * - uses: memory allocation functions provided by a run-time (either
 *   management of static memory, or something like parsec)
 * - uses: scheduler modules that implement the scheduling policy
 *
 */

#ifndef SL_H
#define SL_H

#include <cos_defkernel_api.h>
#include <ps.h>
#include <res_spec.h>
#include <sl_mod_policy.h>
#include <sl_plugins.h>
#include <sl_consts.h>
#include <heap.h>

/* Critical section (cs) API to protect scheduler data-structures */
struct sl_cs {
	union sl_cs_intern {
		struct {
			thdcap_t owner : 31;
			u32_t    contention : 1;
		} PS_PACKED   s;
		unsigned long v;
	} u;
};

struct sl_global {
	struct sl_cs lock;

	thdcap_t       sched_thdcap;
	tcap_t         sched_tcap;
	arcvcap_t      sched_rcv;
	struct sl_thd *sched_thd;
	struct sl_thd *idle_thd;

	int         cyc_per_usec;
	cycles_t    period;
	cycles_t    timer_next;
	tcap_time_t timeout_next;
};

extern struct sl_global sl_global_data;

static inline struct sl_global *
sl__globals(void)
{
	return &sl_global_data;
}

static inline void
sl_thd_setprio(struct sl_thd *t, tcap_prio_t p)
{
	t->prio = p;
}

static inline struct sl_thd *
sl_thd_lkup(thdid_t tid)
{
	assert(tid != 0);
	if (unlikely(tid > MAX_NUM_THREADS)) return NULL;
	return sl_mod_thd_get(sl_thd_lookup_backend(tid));
}

static inline thdid_t
sl_thdid(void)
{
	thdid_t tid = cos_thdid();

	assert(tid != 0);
	assert(tid < MAX_NUM_THREADS);

	return tid;
}


static inline struct sl_thd *
sl_thd_curr(void)
{
	return sl_thd_lkup(sl_thdid());
}

/* are we the owner of the critical section? */
static inline int
sl_cs_owner(void)
{
	return sl__globals()->lock.u.s.owner == sl_thd_thdcap(sl_thd_curr());
}

/* ...not part of the public API */
/*
 * @csi: current critical section value
 * @cached: a cached copy of @csi
 * @curr: currently executing thread
 * @tok: scheduler synchronization token for cos_defswitch
 *
 * @ret:
 *     (Caller of this function should retry for a non-zero return value.)
 *     1 for cas failure or after successful thread switch to thread that owns the lock.
 *     -ve from cos_defswitch failure, allowing caller for ex: the scheduler thread to
 *     check if it was -EBUSY to first recieve pending notifications before retrying lock.
 */
int sl_cs_enter_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, thdcap_t curr, sched_tok_t tok);
/*
 * @csi: current critical section value
 * @cached: a cached copy of @csi
 * @tok: scheduler synchronization token for cos_defswitch
 *
 * @ret: returns 1 if we need a retry, 0 otherwise
 */
int sl_cs_exit_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, sched_tok_t tok);

/* Enter into the scheduler critical section */
static inline int
sl_cs_enter_nospin(void)
{
	union sl_cs_intern csi, cached;
	struct sl_thd *    t = sl_thd_curr();
	sched_tok_t        tok;

	assert(t);
	tok      = cos_sched_sync();
	csi.v    = sl__globals()->lock.u.v;
	cached.v = csi.v;

	if (unlikely(csi.s.owner)) {
		return sl_cs_enter_contention(&csi, &cached, sl_thd_thdcap(t), tok);
	}

	csi.s.owner = sl_thd_thdcap(t);
	if (!ps_cas(&sl__globals()->lock.u.v, cached.v, csi.v)) return 1;

	return 0;
}

/* Enter into scheduler cs from a non-sched thread context */
static inline void
sl_cs_enter(void)
{
	while (sl_cs_enter_nospin())
		;
}

/*
 * Enter into scheduler cs from scheduler thread context
 * @ret: returns -EBUSY if sched thread has events to process and cannot switch threads, 0 otherwise.
 */
static inline int
sl_cs_enter_sched(void)
{
	int ret;

	while ((ret = sl_cs_enter_nospin())) {
		if (ret == -EBUSY) break;
	}

	return ret;
}

/*
 * Release the scheduler critical section, switch to the scheduler
 * thread if there is pending contention
 */
static inline void
sl_cs_exit(void)
{
	union sl_cs_intern csi, cached;
	sched_tok_t        tok;

	assert(sl_cs_owner());

retry:
	tok      = cos_sched_sync();
	csi.v    = sl__globals()->lock.u.v;
	cached.v = csi.v;

	if (unlikely(csi.s.contention)) {
		if (sl_cs_exit_contention(&csi, &cached, tok)) goto retry;
		return;
	}

	if (!ps_cas(&sl__globals()->lock.u.v, cached.v, 0)) goto retry;
}

/*
 * if tid == 0, just block the current thread; otherwise, create a
 * dependency from this thread on the target tid (i.e. when the
 * scheduler chooses to run this thread, we will run the dependency
 * instead (note that "dependency" is transitive).
 */
void sl_thd_block(thdid_t tid);
/*
 * @abs_timeout: absolute timeout at which thread should be woken-up.
 *               if abs_timeout == 0, block forever = sl_thd_block()
 *
 * @returns: 0 if the thread is woken up by external events before timeout.
 *	     +ve - number of cycles elapsed from abs_timeout before the thread
 *		   was woken up by Timeout module.
 */
cycles_t sl_thd_block_timeout(thdid_t tid, cycles_t abs_timeout);
/*
 * blocks for a timeout = next replenishment period of the task.
 * Note: care should be taken to not interleave this with sl_thd_block_timeout().
 *       It may be required to interleave, in such cases, timeout values in
 *       sl_thd_block_timeout() should not be greater than or equal to
 *       the task's next replenishment period.
 *
 * @returns: 0 if the thread is woken up by external events before timeout.
 *           +ve - number of periods elapsed. (1 if it wokeup exactly at timeout = next period)
 */
unsigned int sl_thd_block_periodic(thdid_t tid);
int          sl_thd_block_no_cs(struct sl_thd *t, sl_thd_state_t block_type, cycles_t abs_timeout);

/* wakeup a thread that has (or soon will) block */
void sl_thd_wakeup(thdid_t tid);
int  sl_thd_wakeup_no_cs(struct sl_thd *t);
/* wakeup thread and do not remove from timeout queue if blocked on timeout */
int  sl_thd_wakeup_no_cs_rm(struct sl_thd *t);

void sl_thd_yield(thdid_t tid);
void sl_thd_yield_cs_exit(thdid_t tid);

/* The entire thread allocation and free API */
struct sl_thd *sl_thd_alloc(cos_thd_fn_t fn, void *data);
struct sl_thd *sl_thd_aep_alloc(cos_aepthd_fn_t fn, void *data, int own_tcap);
/*
 * This API creates a sl_thd object for this child component.
 * @comp: component created using cos_defkernel_api which includes initthd (with/without its own tcap & rcvcap).
 */
struct sl_thd *sl_thd_comp_init(struct cos_defcompinfo *comp, int is_sched);
void           sl_thd_free(struct sl_thd *t);

void sl_thd_param_set(struct sl_thd *t, sched_param_t sp);

static inline microsec_t
sl_cyc2usec(cycles_t cyc)
{
	return cyc / sl__globals()->cyc_per_usec;
}

static inline microsec_t
sl_usec2cyc(microsec_t usec)
{
	return usec * sl__globals()->cyc_per_usec;
}

static inline cycles_t
sl_now(void)
{
	return ps_tsc();
}

static inline microsec_t
sl_now_usec(void)
{
	return sl_cyc2usec(sl_now());
}

/*
 * Time and timeout API.
 *
 * This can be used by the scheduler policy module *and* by the
 * surrounding component code.  To avoid race conditions between
 * reading the time, and setting a timeout, we avoid relative time
 * measurements.  sl_now gives the current cycle count that is on an
 * absolute timeline.  The periodic function sets a period that can be
 * used when a timeout has happened, the relative function sets a
 * timeout relative to now, and the oneshot timeout sets a timeout on
 * the same absolute timeline as returned by sl_now.
 */
void sl_timeout_period(cycles_t period);

static inline cycles_t
sl_timeout_period_get(void)
{
	return sl__globals()->period;
}

static inline void
sl_timeout_oneshot(cycles_t absolute_us)
{
	sl__globals()->timer_next   = absolute_us;
	sl__globals()->timeout_next = tcap_cyc2time(absolute_us);
}

static inline void
sl_timeout_relative(cycles_t offset)
{
	sl_timeout_oneshot(sl_now() + offset);
}

static inline void
sl_timeout_expended(microsec_t now, microsec_t oldtimeout)
{
	cycles_t offset;

	assert(now >= oldtimeout);

	/* in virtual environments, or with very small periods, we might miss more than one period */
	offset = (now - oldtimeout) % sl_timeout_period_get();
	sl_timeout_oneshot(now + sl_timeout_period_get() - offset);
}

/* to get timeout heap. not a public api */
struct heap *sl_timeout_heap(void);

/* wakeup any blocked threads! */
static inline void
sl_timeout_wakeup_expired(cycles_t now)
{
	if (!heap_size(sl_timeout_heap())) return;

	do {
		struct sl_thd *tp, *th;

		tp = heap_peek(sl_timeout_heap());
		assert(tp);

		/* FIXME: logic for wraparound in current tsc */
		if (likely(tp->timeout_cycs > now)) break;

		th = heap_highest(sl_timeout_heap());
		assert(th && th == tp);
		th->timeout_idx = -1;

		assert(th->wakeup_cycs == 0);
		th->wakeup_cycs = now;
		sl_thd_wakeup_no_cs_rm(th);
	} while (heap_size(sl_timeout_heap()));
}

static inline int
sl_thd_activate(struct sl_thd *t, sched_tok_t tok)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci  = &dci->ci;

	if (t->properties & SL_THD_PROPERTY_SEND) {
		return cos_sched_asnd(t->sndcap, sl__globals()->timeout_next, sl__globals()->sched_rcv, tok);
	} else if (t->properties & SL_THD_PROPERTY_OWN_TCAP) {
		return cos_switch(sl_thd_thdcap(t), sl_thd_tcap(t), t->prio,
				  sl__globals()->timeout_next, sl__globals()->sched_rcv, tok);
	} else {
		return cos_defswitch(sl_thd_thdcap(t), t->prio, sl__globals()->timeout_next, tok);
	}
}

/*
 * Do a few things: 1. take the critical section if it isn't already
 * taken, 2. call schedule to find the next thread to run, 3. release
 * the critical section (note this will cause visual asymmetries in
 * your code if you call sl_cs_enter before this function), and
 * 4. switch to the given thread.  It hides some races, and details
 * that would make this difficult to write repetitively.
 *
 * Preconditions: if synchronization is required with code before
 * calling this, you must call sl_cs_enter before-hand (this is likely
 * a typical case).
 *
 * Return: the return value from cos_switch.  The caller must handle
 * this value correctly.
 *
 * A common use-case is:
 *
 * sl_cs_enter();
 * scheduling_stuff()
 * sl_cs_exit_schedule();
 *
 * ...which correctly handles any race-conditions on thread selection and
 * dispatch.
 */
static inline int
sl_cs_exit_schedule_nospin_arg(struct sl_thd *to)
{
	struct cos_defcompinfo *dci = cos_defcompinfo_curr_get();
	struct cos_compinfo *ci = &dci->ci;
	struct sl_thd_policy *pt;
	struct sl_thd *       t;
	struct sl_global *    globals = sl__globals();
	sched_tok_t           tok;
	cycles_t              now;
	s64_t                 offset;

	/* Don't abuse this, it is only to enable the tight loop around this function for races... */
	if (unlikely(!sl_cs_owner())) sl_cs_enter();

	tok    = cos_sched_sync();
	now    = sl_now();
	offset = (s64_t)(globals->timer_next - now);
	if (globals->timer_next && offset <= 0) sl_timeout_expended(now, globals->timer_next);
	sl_timeout_wakeup_expired(now);

	/*
	 * Once we exit, we can't trust t's memory as it could be
	 * deallocated/modified, so cache it locally.  If these values
	 * are out of date, the scheduler synchronization tok will
	 * catch it.  This is a little twitchy and subtle, so lets put
	 * it in a function, here.
	 */
	if (unlikely(to)) {
		t = to;
		if (t->state != SL_THD_RUNNABLE) to= NULL;
	}
	if (likely(!to)) {
		pt = sl_mod_schedule();
		if (unlikely(!pt))
			t = sl__globals()->idle_thd;
		else
			t = sl_mod_thd_get(pt);
	}

	if (t->properties & SL_THD_PROPERTY_OWN_TCAP) {
		assert(t->budget && t->period);

		if (t->last_replenish == 0 || t->last_replenish + t->period <= now) {
			tcap_res_t currbudget;

			t->last_replenish = now;
			currbudget        = (tcap_res_t)cos_introspect(ci, sl_thd_tcap(t), TCAP_GET_BUDGET);
			/* TODO: need to change logic for SNDCAP with tcap_delegate, and error handling */
			if (currbudget < t->budget && cos_tcap_transfer(sl_thd_rcvcap(t), sl__globals()->sched_tcap, (t->budget - currbudget), t->prio)) assert(0);
		}
	}

	assert(t->state == SL_THD_RUNNABLE || t->state == SL_THD_WOKEN);
	sl_cs_exit();

	/* TODO: handle `-EPERM` in cos_switch() to interrupt thread or cos_asnd to child comp with its own tcap here. */
	return sl_thd_activate(t, tok);
}

static inline int
sl_cs_exit_schedule_nospin(void)
{
	return sl_cs_exit_schedule_nospin_arg(NULL);
}

static inline void
sl_cs_exit_schedule(void)
{
	while (sl_cs_exit_schedule_nospin())
		;
}

static inline void
sl_cs_exit_switchto(struct sl_thd *to)
{
	/*
	 * We only try once, so it is possible that we don't end up
	 * switching to the desired thread.  However, this is always a
	 * case that the caller has to consider if the current thread
	 * has a higher priority than the "to" thread.
	 */
	if (sl_cs_exit_schedule_nospin_arg(to)) {
		sl_cs_exit_schedule();
	}
}

/*
 * Initialization protocol in cos_init: initialization of
 * library-internal data-structures, and then the ability for the
 * scheduler thread to start its scheduling loop.
 *
 * sl_init();
 * sl_*;            <- use the sl_api here
 * ...
 * sl_sched_loop(); <- loop here
 */
void sl_init(void);
void sl_sched_loop(void);

#endif /* SL_H */
