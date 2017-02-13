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

/* Critical section (cs) API to protect scheduler data-structures */
struct sl_cs {
	union sl_cs_intern {
		struct {
			thdcap_t owner      :31;
			u32_t    contention :1;
		} PS_PACKED s;
		unsigned long v;
	} u;
};

struct sl_global {
	struct sl_cs   lock;

	thdcap_t       sched_thdcap;
	struct sl_thd *sched_thd;
	struct sl_thd *idle_thd;

	int            cyc_per_usec;
	cycles_t       period;
	cycles_t       timer_next;
};

extern struct sl_global sl_global_data;

static inline struct sl_global *
sl__globals(void)
{ return &sl_global_data; }

static inline void
sl_thd_setprio(struct sl_thd *t, tcap_prio_t p)
{ t->prio = p; }

static inline struct sl_thd *
sl_thd_lkup(thdid_t tid)
{ return sl_mod_thd_get(sl_thd_lookup_backend(tid)); }

static inline struct sl_thd *
sl_thd_curr(void)
{ return sl_thd_lkup(cos_thdid()); }

/* are we the owner of the critical section? */
static inline int
sl_cs_owner(void)
{ return sl__globals()->lock.u.s.owner == sl_thd_curr()->thdcap; }

/* ...not part of the public API */
void sl_cs_enter_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, thdcap_t curr, sched_tok_t tok);
int sl_cs_exit_contention(union sl_cs_intern *csi, union sl_cs_intern *cached, sched_tok_t tok);

/* Enter into the scheduler critical section */
static inline void
sl_cs_enter(void)
{
	union sl_cs_intern csi, cached;
	struct sl_thd     *t = sl_thd_curr();
	sched_tok_t        tok;

	assert(t);
retry:
	tok      = cos_sched_sync();
	csi.v    = sl__globals()->lock.u.v;
	cached.v = csi.v;

	if (unlikely(csi.s.owner)) {
		sl_cs_enter_contention(&csi, &cached, t->thdcap, tok);
		goto retry;
	}

	csi.s.owner = t->thdcap;
	if (!ps_cas(&sl__globals()->lock.u.v, cached.v, csi.v)) goto retry;

	return;
}

/*
 * Release the scheduler critical section, switch to the scheduler
 * thread if there is pending contention
 */
static inline void
sl_cs_exit(void)
{
	union sl_cs_intern csi, cached;
	sched_tok_t tok;

retry:
	tok      = cos_sched_sync();
	csi.v    = sl__globals()->lock.u.v;
	cached.v = csi.v;

	if (unlikely(csi.s.contention)) {
		if (sl_cs_exit_contention(&csi, &cached, tok)) goto retry;
		return;
	}

	if (!ps_cas(&sl__globals()->lock.u.v, cached.v, 0))    goto retry;
}

static inline cycles_t
sl_now(void)
{ return ps_tsc(); }

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
sl_cs_exit_schedule_nospin(void)
{
	struct sl_thd_policy *pt;
	struct sl_thd        *t;
	struct sl_global     *globals = sl__globals();
	thdcap_t       thdcap;
	tcap_prio_t    prio;
	sched_tok_t    tok;
	cycles_t       now;
	s64_t          offset;

	if (unlikely(!sl_cs_owner())) sl_cs_enter();

	tok    = cos_sched_sync();
	now    = sl_now();
	offset = (s64_t)(globals->timer_next - now);
	if (globals->timer_next && offset <= 0) sl_timeout_mod_expended(now, globals->timer_next);

	/*
	 * Once we exit, we can't trust t's memory as it could be
	 * deallocated/modified, so cache it locally.  If these values
	 * are out of date, the scheduler synchronization tok will
	 * catch it.  This is a little twitchy and subtle, so lets put
	 * it in a function, here.
	 */
	pt     = sl_mod_schedule();
	if (unlikely(!pt)) t = sl__globals()->idle_thd;
	else               t = sl_mod_thd_get(pt);
	thdcap = t->thdcap;
	prio   = t->prio;

	sl_cs_exit();

	/* TODO: enable per-thread tcaps for interrupt threads */
	return cos_defswitch(thdcap, prio, sl__globals()->timer_next, tok);
}

static inline void
sl_cs_exit_schedule(void)
{ while (sl_cs_exit_schedule_nospin()) ; }

/*
 * if tid == 0, just block the current thread; otherwise, create a
 * dependency from this thread on the target tid (i.e. when the
 * scheduler chooses to run this thread, we will run the dependency
 * instead (note that "dependency" is transitive).
 */
void sl_thd_block(thdid_t tid);
/* wakeup a thread that has (or soon will) block */
void sl_thd_wakeup(thdid_t tid);
void sl_thd_yield(thdid_t tid);

/* The entire thread allocation and free API */
struct sl_thd *sl_thd_alloc(cos_thd_fn_t fn, void *data);
struct sl_thd *sl_thd_comp_alloc(struct cos_defcompinfo *comp);
void sl_thd_free(struct sl_thd *t);

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
{ return sl__globals()->period; }

static inline void
sl_timeout_oneshot(cycles_t absolute_us)
{ sl__globals()->timer_next = absolute_us; }

static inline void
sl_timeout_relative(cycles_t offset)
{ sl_timeout_oneshot(sl_now() + offset); }

static inline microsec_t
sl_cyc2usec(cycles_t cyc)
{ return cyc / sl__globals()->cyc_per_usec; }

static inline microsec_t
sl_usec2cyc(microsec_t usec)
{ return usec * sl__globals()->cyc_per_usec; }

void sl_thd_param_set(struct sl_thd *t, sched_param_t sp);

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


#endif	/* SL_H */
