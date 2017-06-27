/**
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Copyright 2008 by Boston University.
 * Author: Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Copyright 2009, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#include <cos_config.h>
#ifdef LINUX_ON_IDLE
#define IDLE_TO_LINUX
#endif

//#define UPCALL_TIMING 1

#define COS_FMT_PRINT

#include <cos_component.h>
#include <cos_sched_ds.h>
#include <cos_scheduler.h>
#include <cos_time.h>
#include <print.h>

#include <cos_sched_tk.h>

#include <sched.h>
#include <sched_hier.h>

#include <bitmap.h>
#include <ck_ring.h>
#include <ck_spinlock.h>

//#define TIMER_ACTIVATE
#include <timer.h>
#include <errno.h>

//#define SCHED_DEBUG
#ifdef SCHED_DEBUG
#define PRINTD(s, args...) printc(s, args);
#else
#define PRINTD(s, args...)
#endif

#include <sched_timing.h>

struct sched_base_per_core {
	volatile u64_t ticks;

        /* When the wakeup thread (in charge of higher-level timing) should be woken */
	volatile u64_t wakeup_time;
	volatile u64_t child_wakeup_time;
	struct sched_thd *wakeup_thd;

        /*
	 * timer is the timer tick thread for a root scheduler, or the child
	 * event thread for a child scheduler.  init is the initial bootup
	 * thread for a root scheduler.  init is meaningless for a child
	 * scheduler.  idle is the idle thread, again only meaningful for root
	 * schedulers.
	 */
	struct sched_thd *timer, *init, *idle, *IPI_handler;
	struct sched_thd blocked;
	struct sched_thd upcall_deactive;
	struct sched_thd graveyard;
	int IPI_acap;
	long long report_evts[REVT_LAST];
} CACHE_ALIGNED;

PERCPU_ATTR(static, struct sched_base_per_core, sched_base_state);

static enum {SCHED_CHILD, SCHED_ROOT} sched_type = SCHED_ROOT;
static inline int sched_is_root(void) { return sched_type == SCHED_ROOT; }
static inline int sched_is_child(void) { return !sched_is_root(); }

/* What is the spdid of the booter component? */
#define BOOT_SPD 5

//#define FPRR_REPORT_EVTS

#ifdef FPRR_REPORT_EVTS

static char *revt_names[] = {
	"null event",
	"switch threads",
	"acap active event",
	"acap ready event",
	"acap pending event",
	"event cycle update",
	"scheduling using a dependency",
	"thread blocking",
	"thread waking",
	"component lock take (call)",
	"component lock take (actual attempt)",
	"component lock take contention",
	"component lock release",
	"timer tick",
	"timer tick switch thread loop",
	"idle loop interpreting event",
	"idle loop trying to schedule event",
	"event completion, uc into sched",
	"iterations through the block loop",
	"iterations through the wake loop",
	"iterations through the timer loop",
	"iterations through the component_take loop",
	"iterations through the event completion loop",
	"iterations through the timeout loop",
	"iterations through the idle loop",
	"re-asserting dependency",
	"premature unblock, retrying block",
	"block with dependency",
	"dependency on blocked thread",
	"schedule will explicit target (via dependency)",
	"parent scheduler blocking child scheduler",
	"parent scheduler sending non-thread block/wake event",
	"parent scheduler sending thread block/wake event",
	"parent scheduler deactivating child sched thread",
	"parent scheduler resuming thread for child sched",
	"parent scheduler has redundant resume for child sched",
	"child scheduler processing event with possible idle",
	"child scheduler processing event with pending work",
	"child scheduler processing event: block thread",
	"child scheduler processing event: wake thread",
	"child scheduler processing event: other (e.g. evt thd)",
	"child scheduler calls switch_thread",
	"child scheduler reschedules due to pending cevt",
	""
};

/* Timers */
enum {
	TIMER_SCHED = 0,
	TIMER_FPRR,
	TIMER_SWTCH,
	TIMER_MAX
};
STATIC_TIMER_RECORDS(recs, TIMER_MAX);//should this be per_core?

static void report_event(report_evt_t evt)
{
	if (unlikely(evt >= REVT_LAST)) return;

	PERCPU_GET(sched_base_state)->report_evts[evt]++;
}

static void report_output(void)
{
	int i, j;

	for (j = 0; j < NUM_CPU; j++) {
		printc("  <<<Core %d>>> All counters:\n", j);
		for (i = 0 ; i < REVT_LAST ; i++) {
			printc("\t%s: %lld\n", revt_names[i], PERCPU_GET_TARGET(sched_base_state, j)->report_evts[i]);
			PERCPU_GET_TARGET(sched_base_state, j)->report_evts[i] = 0;
		}
	}

//	mman_print_stats();
}
#else
#define report_event(e)
#define report_output()
#endif

static void report_thd_accouting(void)
{
	struct sched_thd *t;
	struct sched_base_per_core *sched_base = PERCPU_GET(sched_base_state);
	runqueue_print();

	if (sched_is_child()) {
		struct sched_accounting *sa = sched_get_accounting(sched_base->timer);

		printc("\nChild timer thread (thd, ticks):\n");
		printc("\t%d, %ld\n", sched_base->timer->id, sa->ticks - sa->prev_ticks);
		sa->prev_ticks = sa->ticks;
	}

	printc("\nBlocked threads (thd, prio, ticks):\n");
	for (t = FIRST_LIST(&sched_base->blocked, prio_next, prio_prev) ;
	     t != &sched_base->blocked ;
	     t = FIRST_LIST(t, prio_next, prio_prev)) {
		struct sched_accounting *sa = sched_get_accounting(t);
		unsigned long diff = sa->ticks - sa->prev_ticks;

		if (diff || sa->cycles) {
			printc("\t%d, %d, %ld+%ld/%ld\n", t->id,
			       sched_get_metric(t)->priority, diff,
			       (unsigned long)sa->cycles, (unsigned long)CYC_PER_TICK);
			sa->prev_ticks = sa->ticks;
			sa->cycles = 0;
		}
	}
	printc("\nInactive upcalls (thd, prio, ticks):\n");
	for (t = FIRST_LIST(&sched_base->upcall_deactive, prio_next, prio_prev) ;
	     t != &sched_base->upcall_deactive ;
	     t = FIRST_LIST(t, prio_next, prio_prev)) {
		struct sched_accounting *sa = sched_get_accounting(t);
		unsigned long diff = sa->ticks - sa->prev_ticks;

		if (diff || sa->cycles) {
			printc("\t%d, %d, %ld+%ld/%ld\n", t->id,
			       sched_get_metric(t)->priority, diff,
			       (unsigned long)sa->cycles, (unsigned long)CYC_PER_TICK);
			sa->prev_ticks = sa->ticks;
			sa->cycles = 0;
		}
	}
	printc("\n");
	report_output();

#ifdef TIMER_ACTIVATE
	{
		int i;

		for (i = 0 ; i < TIMER_MAX ; i++) {
			unsigned long avg = 0, max = 0, min = 0;
			timer_report(recs, i, &avg, &max, &min);
			printc("avg: %ld, min: %ld, max: %ld\n", avg, min, max);
		}
	}
#endif
}

static void activate_child_sched(struct sched_thd *g);

static inline void fp_resume_thd(struct sched_thd *t)
{
	assert(sched_thd_blocked(t));
	assert(!sched_thd_free(t));
	assert(!sched_thd_ready(t));

	t->flags &= ~THD_BLOCKED;
	t->flags |= THD_READY;
	REM_LIST(t, prio_next, prio_prev);
	/* child threads aren't reported to the scheduler */
	if (!sched_thd_member(t)) {
		assert(sched_is_root() || (t != PERCPU_GET(sched_base_state)->timer && !sched_thd_phantom(t)));
		thread_wakeup(t);
	}
	/* Is the member _not_ already on the list for the group? */
	else if (EMPTY_LIST(t, cevt_next, cevt_prev) /* && is member */) {
		struct sched_thd *g = sched_get_grp(t);
		ADD_LIST(g, t, cevt_next, cevt_prev);
		activate_child_sched(g);
		report_event(PARENT_CHILD_RESUME);
	} else {
		report_event(PARENT_CHILD_REDUNDANT_RESUME);
	}
}

static void fp_activate_upcall(struct sched_thd *uc)
{
	if (sched_thd_inactive_evt(uc)) {
		uc->flags &= ~THD_UC_READY;
		uc->flags |= THD_READY;
		REM_LIST(uc, prio_next, prio_prev); //done in move_end_runnable
		assert(sched_is_root() || (uc != PERCPU_GET(sched_base_state)->timer && !sched_thd_phantom(uc)));
		thread_wakeup(uc);
	}
}

static void fp_deactivate_upcall(struct sched_thd *uc)
{
	uc->flags &= ~THD_READY;
	uc->flags |= THD_UC_READY;
	thread_block(uc);
	assert(EMPTY_LIST(uc, prio_next, prio_prev));
	ADD_LIST(&PERCPU_GET(sched_base_state)->upcall_deactive, uc, prio_next, prio_prev);
}

/* scheduler lock should already be taken */
static void evt_callback(struct sched_thd *t, u8_t flags, u32_t cpu_usage)
{
	assert(!sched_thd_member(t));

	if (sched_thd_free(t) || sched_thd_dying(t)) return;

	if (flags & (COS_SCHED_EVT_ACAP_ACTIVE|COS_SCHED_EVT_ACAP_READY|COS_SCHED_EVT_ACAP_PEND)) {
		if (sched_thd_grp(t)) {
			activate_child_sched(t);
			t->cevt_flags |= SCHED_CEVT_OTHER;
		} else if (flags & COS_SCHED_EVT_ACAP_ACTIVE) {
			report_event(ACAP_ACTIVE);
			fp_activate_upcall(t);
		} else if (flags & COS_SCHED_EVT_ACAP_READY) {
			assert(sched_get_current() != t);
			report_event(ACAP_READY);
			fp_deactivate_upcall(t);
		} else if (flags & COS_SCHED_EVT_ACAP_PEND) {
			BUG();
		}
	} else {
		report_event(ACAP_CYCLE);
	}
	if (!sched_thd_phantom(t)) time_elapsed(t, cpu_usage);

	return;
}

static void evt_callback_print(struct sched_thd *t, u8_t flags, u32_t cpu_usage)
{
	PRINTD("evt callback (curr %d): thd %d, flags %x, usage %d\n", sched_get_current()->id, t->id, flags, cpu_usage);
	evt_callback(t, flags, cpu_usage);
}

static struct sched_thd *resolve_dependencies(struct sched_thd *next)
{
	struct sched_thd *dep;

	/* Take dependencies into account */
	if ((dep = sched_thd_dependency(next))) {
		assert(!sched_thd_free(dep));
		assert(!(next->flags & (COS_SCHED_ACAP_WAIT|COS_SCHED_TAILCALL)));
		assert(!sched_thd_blocked(dep));
		if (sched_thd_inactive_evt(dep)) {
			printc("Thread %d resolving dependency to %d, but the latter is waiting for an interrupt\n", next->id, dep->id);
		}
		assert(!sched_thd_inactive_evt(dep));
		assert(sched_thd_ready(dep));
		assert(dep != next);
		report_event(SCHED_DEPENDENCY);
		next = dep;

		/* At this point it's possible that next == current.
		 * If we hold the component lock requested by the
		 * highest prio thread, then we are the depended on
		 * thread and should continue executing. */
	}
	return next;
}

/*
 * Important: assume that cos_sched_lock_take() has been called.  The
 * reason for this assumption is so that outer (calling) code can
 * execute instructions within the same critical sections as the code
 * that switches the thread.  flags are to be passed to
 * cos_switch_thread, and if they include tailcall, or acap wait,
 * this function will ensure that the current head isn't chosen to
 * run.  evt is the event number that will be used to increment for
 * each iteration of the loop, or if it is -1, no event will be
 * incremented.
 */
static int sched_switch_thread_target(int flags, report_evt_t evt, struct sched_thd *target)
{
	/* Current can be NULL if the current thread is being killed */
	struct sched_thd *current = sched_get_current();
	int ret;

	do {
		struct sched_thd *next;
		TIMER_INIT(t, recs, TIMER_SCHED);
		TIMER_INIT(tfp, recs, TIMER_FPRR);

		timer_start(&t);
		assert(cos_sched_lock_own());

		/*
		 * This is subtle: an event might happen _after_ we
		 * check the pending flag here.  If so, then when we
		 * invoke switch_thread, the kernel will return
		 * COS_SCHED_RET_AGAIN, and this will be run again.
		 * In this way, we never miss an event for a
		 * scheduling decision.  Also, we are trying to keep
		 * accurate counts of the number of cycles executed
		 * for the previous thread.  Doing so means knowing
		 * how many cycles were consumed by interrupt/upcall
		 * execution.  We have the loop here so that if there
		 * is an event that happens between when events are
		 * processed (getting a count of cycles spent for
		 * upcalls), and when the time stamp is taken (to find
		 * the amount of cycles since we last explicitly
		 * scheduled.
		 */
		if (cos_sched_pending_event()) {
			cos_sched_clear_events();
			cos_sched_process_events(evt_callback, 0);
		}

		if (!target) {
			/*
			 * If current is an upcall that wishes to terminate
			 * its execution upon switching to the next thread,
			 * then it will pass in one of the following flags.
			 * If that is the case, then we wish to ask the
			 * scheduler to return a thread that is _not_, even if
			 * current is the highest priority thread because
			 * current will terminate execution with the switch.
			 */
			if (flags & (COS_SCHED_ACAP_WAIT|COS_SCHED_TAILCALL)) {
				/* we don't want next to be us! We are an
				 * upcall completing execution */
				timer_start(&tfp);
				next = schedule(current);
				timer_end(&tfp);
				assert(sched_is_root() || PERCPU_GET(sched_base_state)->timer != next);
				assert(next != current);
				assert(!sched_thd_member(next));
			} else {
				timer_start(&tfp);
				next = schedule(NULL);
				timer_end(&tfp);
				assert(sched_is_root() || PERCPU_GET(sched_base_state)->timer != next);
				assert(!sched_thd_member(next));
				/* if we are the next thread and no
				 * dependencies have been introduced (i.e. we
				 * are waiting on a component-lock for
				 * another thread), then we're done */
				if (next == current && !sched_thd_dependency(current)) goto done;
			}
		} else {
			report_event(SCHED_TARGETTED_DEPENDENCY);
			next = target;
		}
		next = resolve_dependencies(next);
		if (next == current) goto done;
		if (sched_is_child() && unlikely(next == PERCPU_GET(sched_base_state)->idle)) {
			/* This is a kludge: child schedulers don't
			 * have idle threads...instead they just use
			 * the event/timer thread */
			/* We are in the timer/child event thread! */
			if (current == PERCPU_GET(sched_base_state)->timer) goto done;
			next = PERCPU_GET(sched_base_state)->timer;
		}
		if (sched_thd_grp(next)) flags |= COS_SCHED_CHILD_EVT;
		assert(!sched_thd_blocked(next));
		report_event(SWITCH_THD);
		timer_end(&t);
		ret = cos_switch_thread_release(next->id, flags);

		assert(ret != COS_SCHED_RET_ERROR);
		if (COS_SCHED_RET_CEVT == ret) { report_event(CEVT_RESCHED); }
		/* success, or we need to check for more child events:
		 * exit the loop! */
		if (likely(COS_SCHED_RET_SUCCESS == ret) || COS_SCHED_RET_CEVT == ret) break;

		cos_sched_lock_take();
		if (evt != NULL_EVT) { report_event(evt); }
		/* keep looping if we were scheduling using old info */
	} while (unlikely(COS_SCHED_RET_SUCCESS != ret));

	return 0;
done:
	cos_sched_lock_release();

	return 0;
}

static inline int sched_switch_thread(int flags, report_evt_t evt)
{
	return sched_switch_thread_target(flags, evt, NULL);
}

static void fp_pre_wakeup(struct sched_thd *t);
static void fp_wakeup(struct sched_thd *thd, spdid_t spdid);

static void sched_process_wakeups(void)
{
	struct sched_thd *t, *next;
	u64_t lowest_child = 0;

	/* Wakeup the event thread? */
	if (PERCPU_GET(sched_base_state)->wakeup_time && PERCPU_GET(sched_base_state)->ticks >= PERCPU_GET(sched_base_state)->wakeup_time && likely(PERCPU_GET(sched_base_state)->wakeup_thd)) {
		if (PERCPU_GET(sched_base_state)->wakeup_thd->wake_cnt < 2) {
			PERCPU_GET(sched_base_state)->wakeup_time = 0;
			fp_pre_wakeup(PERCPU_GET(sched_base_state)->wakeup_thd);
			/* If the event thread has not blocked yet, then don't
			 * wake it fully */
			if (sched_thd_blocked(PERCPU_GET(sched_base_state)->wakeup_thd)) fp_wakeup(PERCPU_GET(sched_base_state)->wakeup_thd, 0);
		} else {
			assert(!sched_thd_blocked(PERCPU_GET(sched_base_state)->wakeup_thd));
		}
	}

	/*
	 * Find child scheduler threads and monitor for timeouts
	 * FIXME: should organize child schedulers into sorted wakeup
	 * list instead of using a linear walk through all blocked
	 * threads here.
	 */
	for (t = FIRST_LIST(&PERCPU_GET(sched_base_state)->blocked, prio_next, prio_prev) ;
	     t != &PERCPU_GET(sched_base_state)->blocked ;
	     t = next) {
		next = FIRST_LIST(t, prio_next, prio_prev);
		/* child scheduler requested wakeup */
		if (sched_thd_grp(t) && t->wakeup_tick) {
			if (t->wakeup_tick <= PERCPU_GET(sched_base_state)->ticks) {
				/* if the child thread has not been executed
				 * since the wakeup expired */
				if (t->wakeup_tick > t->tick) {
					activate_child_sched(t);
					t->cevt_flags |= SCHED_CEVT_OTHER;
				}
				t->wakeup_tick = 0;
			}
			if (t->wakeup_tick && (0 == lowest_child || t->wakeup_tick < lowest_child)) {
				lowest_child = t->wakeup_tick;
			}
		}
	}
	PERCPU_GET(sched_base_state)->child_wakeup_time = lowest_child;
}

static int exit_system = 0;

static void sched_timer_tick(void)
{
	while (1) {
		cos_sched_lock_take();
		report_event(TIMER_TICK);
		if (unlikely((PERCPU_GET(sched_base_state)->ticks % (REPORT_FREQ*TIMER_FREQ)) == ((REPORT_FREQ*TIMER_FREQ)-1))) {
			report_thd_accouting();
			//cos_stats();
		}

		/* are we done running? */
		if (unlikely(PERCPU_GET(sched_base_state)->ticks >= RUNTIME_SEC*TIMER_FREQ+1 || exit_system)) {
			cos_acap_wire(0, COS_HW_TIMER, 0); // disable cos timer
			sched_exit();
			while (COS_SCHED_RET_SUCCESS !=
			       cos_switch_thread_release(PERCPU_GET(sched_base_state)->init->id, COS_SCHED_ACAP_WAIT)) {
				cos_sched_lock_take();
				if (cos_sched_pending_event()) {
					cos_sched_clear_events();
					cos_sched_process_events(evt_callback, 0);
				}
			}
		}
		PERCPU_GET(sched_base_state)->ticks++;
		sched_process_wakeups();
		timer_tick(1);

		sched_switch_thread(COS_SCHED_ACAP_WAIT, TIMER_SWITCH_LOOP);
		/* Tailcall out of the loop */
	}
}

static void fp_event_completion(struct sched_thd *e)
{
	report_event(EVT_CMPLETE);

	while (1) {
		cos_sched_lock_take();
		sched_switch_thread(COS_SCHED_TAILCALL, EVT_CMPLETE_LOOP);
	}
	BUG();

	return;
}

/* type of newly created thread functions */
typedef void (*crt_thd_fn_t)(void *data);

static void fp_timer(void *d)
{
	int acap, srv_acap, tid;
	struct sched_base_per_core *sched_state = PERCPU_GET(sched_base_state);

	sched_state->ticks = 0;
	sched_state->wakeup_time = 0;
	sched_state->child_wakeup_time = 0;

	tid = cos_get_thd_id();
	acap = cos_async_cap_cntl(COS_ACAP_CREATE, cos_spd_id(), cos_spd_id(), tid << 16 | tid);
	 /* cli_acap not used as the server acap is triggered by timer
	  * interrupt. We set the owner of the cli_acap to be the
	  * timer thread itself to prevent other threads invoking the
	  * acap (so only timer int can trigger it). */
	/* cli_acap = acap >> 16; */
	srv_acap = acap & 0xFFFF;
	cos_acap_wire(srv_acap, COS_HW_TIMER, 0);

	printc("Core %ld: Starting timer thread (id %d) with acap %d.\n", cos_cpuid(), tid, srv_acap);

	sched_timer_tick();
	BUG();
}

/* The ring buffer page used for cross-core communication. */
struct xcore_ring_buffer {
	char ring[PAGE_SIZE / sizeof(char)];
} PAGE_ALIGNED;

PERCPU(struct xcore_ring_buffer, xcore_ring_page);

struct xcore_fn_data {
	void *fn;
	int nparams;
	u32_t params[4];
	volatile int ret;
	volatile int finished;
} CACHE_ALIGNED;

struct xcore_ring_item {
	struct xcore_fn_data *data;
};

CK_RING_PROTOTYPE(xcore_ring, xcore_ring_item);

PERCPU(struct ck_ring, percpu_ring);

static inline int exec_fn(int (*fn)(), int nparams, u32_t *params) ;

ck_spinlock_ticket_t xcore_lock = CK_SPINLOCK_TICKET_INITIALIZER;

static inline int xcore_exec(int core_id, void *fn, int nparams, u32_t *params, int wait)
{
	int ret = 0, i, acap;
	struct xcore_ring_item item;
	struct xcore_fn_data data;
	unsigned long long s = 0, e;
	item.data = &data;

	if (unlikely(core_id >= NUM_CPU_COS || nparams > 4 || !fn || !params)) goto error;

	if (core_id == cos_cpuid()) {
		ret = exec_fn(fn, nparams, params);
		goto done;
	}

	struct ck_ring *ring = PERCPU_GET_TARGET(percpu_ring, core_id);
	void *target_buffer  = PERCPU_GET_TARGET(xcore_ring_page, core_id);
	data.fn = fn;
	data.nparams = nparams;

	for (i = 0; i < nparams; i++) {
		data.params[i] = params[i];
	}

	data.finished = 0;

	ck_spinlock_ticket_lock_pb(&xcore_lock, 1);
	while (unlikely(!CK_RING_ENQUEUE_SPSC(xcore_ring, ring, (struct xcore_ring_item *)target_buffer, &item)))
	{
		if (unlikely(s == 0)) rdtscll(s);
		rdtscll(e);
		/* detect unusual delay */
		if ((e - s) > (1 << 30)) {
			printc("cos_sched_base: xcore_exec pushing into ring buffer has abnormal delay (%llu cycles).\n", e - s);
			s = e;
		}
	}
	ck_spinlock_ticket_unlock(&xcore_lock);

	acap = PERCPU_GET_TARGET(sched_base_state, core_id)->IPI_acap;

	if (unlikely(acap <= 0)) BUG();
	/* printc("core %d: sending ipi to core %ld, acap %d\n", cos_cpuid(), core_id, acap); */
	cos_asend(acap);

	if (wait) {
		rdtscll(s);
		while (data.finished == 0)
		{
			rdtscll(e);
			/* detect unusual delay */
			if ((e - s) > (1 << 30)) {
				printc("cos_sched_base: thd %d on core %ld waiting for core %d abnormal remote execution delay (%llu cycles).\n",
				       cos_get_thd_id(), cos_cpuid(), core_id, e - s);
				s = e;
			}
		}
		ret = data.ret;
	}
done:
	return ret;
error:
	return -1;
}

static void fp_idle_loop(void *d)
{
	assert(sched_is_root());
	while(1) {
		/* Unfortunately, we can't make this strong an
		 * assertion.  Instead, we really can only assert that
		 * non-upcall-complete events shouldn't happen
		assert(!cos_sched_pending_event());  */
		if (cos_sched_pending_event()) {
			report_event(IDLE_SCHED);
 			cos_sched_lock_take();
			sched_switch_thread(0, IDLE_SCHED_SWITCH);
		}
//		report_event(IDLE_SCHED_LOOP);
#ifdef IDLE_TO_LINUX
		cos_idle();
#endif
	}
}

extern unsigned long parent_sched_timer_stopclock(void);
unsigned long sched_timer_stopclock(void)
{
#ifdef UPCALL_TIMING
	if (sched_is_child()) return parent_sched_timer_stopclock();
	return cos_sched_timer_cyc();
#else
	return 0;
#endif
}

unsigned int sched_tick_freq(void)
{
	return TIMER_FREQ;
}

unsigned long sched_cyc_per_tick(void)
{
	return CYC_PER_TICK;
}

static void fp_pre_block(struct sched_thd *thd);
static void fp_block(struct sched_thd *thd, spdid_t spdid);

/* used to set a timeout for the timer thread */
void sched_timeout(spdid_t spdid, unsigned long amnt)
{
	unsigned long long abs_timeout;
	struct sched_thd *thd;

	cos_sched_lock_take();

	if (0 == amnt) {
		cos_sched_lock_release();
		return;
	}

	thd = sched_get_mapping(cos_get_thd_id());
	assert(thd);

	abs_timeout = PERCPU_GET(sched_base_state)->ticks + amnt;

	if (0 == PERCPU_GET(sched_base_state)->wakeup_time || abs_timeout < PERCPU_GET(sched_base_state)->wakeup_time) {
		PERCPU_GET(sched_base_state)->wakeup_time = abs_timeout;
	}
	/* If we're the timer thread, lets block, otherwise return */
	if (thd != PERCPU_GET(sched_base_state)->wakeup_thd) {
		cos_sched_lock_release();
		return;
	}

	fp_pre_block(thd);
	/* If we've already been woken up, so be it! */
	if (thd->wake_cnt) {
		assert(thd->wake_cnt == 1);
		cos_sched_lock_release();
		return;
	}
	fp_block(thd, cos_spd_id());
	sched_switch_thread(0, TIMEOUT_LOOP);

	return;
}

/* Assign a timeout thread that will be executed and woken up in
 * accordance to the nearest timeout */
int sched_timeout_thd(spdid_t spdid)
{
	struct sched_thd *thd;

	cos_sched_lock_take();

	thd = sched_get_mapping(cos_get_thd_id());
	if (!thd) {
		cos_sched_lock_release();
		return -1;
	}
	PERCPU_GET(sched_base_state)->wakeup_thd = thd;

	cos_sched_lock_release();
	return 0;
}

/* increment the wake count and do sanity checking*/
static void fp_pre_wakeup(struct sched_thd *t)
{
	assert(t->wake_cnt >= 0 && t->wake_cnt <= 2);
	if (2 == t->wake_cnt) return;
	t->wake_cnt++;
	/* printc("thd %d cnt++ %d ->%d\n", t->id, t->wake_cnt-1, t->wake_cnt); */
	/* if (sched_get_current() != t && /\* Hack around comment "STKMGR: self wakeup" *\/ */
	/*     !sched_thd_dependent(t) && */
	/*     !(sched_thd_blocked(t) || t->wake_cnt == 2)) { */
	/* 	printc("thread %d (from thd %d) has wake_cnt %d\n", */
	/* 	       t->id, cos_get_thd_id(), t->wake_cnt); */
	/* 	BUG(); */
	/* } */
}

/* Assuming the thread is asleep, this will actually wake it (change
 * queues, etc...) */
static void fp_wakeup(struct sched_thd *thd, spdid_t spdid)
{
	if (thd->wake_cnt != 1) {
		printc("fp_wakeup: thd %d waking %d, wake count %d\n",
		       cos_get_thd_id(), thd->id, thd->wake_cnt);
	}
	// this is triggering with wake_cnt == 2
	assert(thd->wake_cnt == 1);
	/* resume thread, thus no blocking component */
	thd->blocking_component = 0;

	fp_resume_thd(thd);
	report_event(THD_WAKE);
}

/*
 * FIXME: should verify that the blocks and wakes come from the same
 * component.  This is the external interface.
 */
int sched_wakeup(spdid_t spdid, unsigned short int thd_id)
{
	int ret;
	struct sched_thd *thd;

	/* printc("thread %d waking up thread %d.\n", cos_get_thd_id(), thd_id); */

	if (!sched_thd_on_current_core(thd_id)) {
		/* Cross-core wakeup */
		cpuid_t cpu = sched_get_thd_core(thd_id);
		u32_t params[2] = {spdid, thd_id};
		if (cpu < 0) goto error_nolock;

		ret = xcore_exec(cpu, sched_wakeup, 2, params, 1);

		goto xcore_done;
	}

	cos_sched_lock_take();

	thd = sched_get_mapping(thd_id);
	if (!thd) goto error;

	/* only increase the count once */
	fp_pre_wakeup(thd);

	if (thd->dependency_thd) {
		assert(sched_thd_dependent(thd));
		assert(thd->ncs_held == 0 || thd->id == cos_get_thd_id());
		thd->flags &= ~THD_DEPENDENCY;
		thd->dependency_thd = NULL;
		assert(!sched_thd_dependent(thd));
	} else {
		assert(!sched_thd_dependent(thd));
		/* If the thd isn't blocked yet (as it was probably preempted
		 * before it could complete the call to block), no reason to
		 * wake it via scheduling.
		 */
		if (!sched_thd_blocked(thd)) goto cleanup;
		/*
		 * We are waking up a thread, which means that if we
		 * are an upcall, we don't want composite to
		 * automatically switch to the preempted thread (which
		 * might be of lower priority than the woken thread).
		 *
		 * TODO: This could be much more complicated: We could
		 * only call this if indeed we did wake up a thread
		 * that has a higher priority than the currently
		 * executing one (upcall excluded).
		 */
		/* FIXME: instead of this explicit call, instead do a
		 * flag to switch_thread */
		cos_sched_cntl(COS_SCHED_BREAK_PREEMPTION_CHAIN, 0, 0);
		fp_wakeup(thd, spdid);
		assert(!sched_thd_dependent(thd));
	}

	assert(!sched_thd_dependent(thd));
	sched_switch_thread(0, WAKE_LOOP);
done:
	return 0;
cleanup:
	cos_sched_lock_release();
	goto done;
error:
	cos_sched_lock_release();
error_nolock:
	return -1;
xcore_done:
	return ret;
}

/* decrement the wake count, do sanity checking, and record at what
 * time the thread has been woken */
static void fp_pre_block(struct sched_thd *thd)
{
	/* A blocked thread can't block... */
	assert(thd->wake_cnt > 0);
	assert(thd->wake_cnt <= 2);
	thd->wake_cnt--;
	/* printc("thd %d wake cnt %d -> %d \n",thd->id,thd->wake_cnt+1, thd->wake_cnt); */
	thd->block_time = PERCPU_GET(sched_base_state)->ticks;
}

static inline void fp_block_thd(struct sched_thd *t)
{
	assert(!sched_thd_free(t));
	assert(!sched_thd_blocked(t));
	assert(t->wake_cnt == 0);

	t->flags &= ~THD_READY;
	t->flags |= THD_BLOCKED;
	/* Child threads aren't reported to the scheduler */
	if (!sched_thd_member(t)) {
		thread_block(t);
		assert(EMPTY_LIST(t, prio_next, prio_prev));
	} else if (EMPTY_LIST(t, cevt_next, cevt_prev) /* && is member */) {
		struct sched_thd *g = sched_get_grp(t);
		ADD_LIST(g, t, cevt_next, cevt_prev);
		activate_child_sched(g);
	}
	ADD_LIST(&PERCPU_GET(sched_base_state)->blocked, t, prio_next, prio_prev);
}

/* Really block the thread (inc. queue manipulation) */
static void fp_block(struct sched_thd *thd, spdid_t spdid)
{
	assert(thd->wake_cnt == 0);
	thd->blocking_component = spdid;

	fp_block_thd(thd);
	report_event(THD_BLOCK);
}

/*
 * FIXME: should verify that the blocks and wakes come from the same
 * component.  This is the externally visible function.
 */
int sched_block(spdid_t spdid, unsigned short int dep_thd)
{
	struct sched_thd *thd, *dep = NULL;
	int ret, first = 1;
	unsigned short int dependency_thd = dep_thd;

	// Added by Gabe 08/19
	if (unlikely(dep_thd == cos_get_thd_id())) return -EINVAL;

	cos_sched_lock_take();
	thd = sched_get_current();
	assert(thd);
	assert(spdid);

	/* We only do dependencies if the dep thread is on the current core. */
	if (dep_thd) {
		if (sched_thd_on_current_core(dep_thd)) {
			dependency_thd = dep_thd;
		} else {
			printc("Core %ld, thd %d: Dependency on a remote core, dep thread %d, from spd %d.\n", cos_cpuid(), cos_get_thd_id(), dep_thd, spdid);
			dependency_thd = 0; /* Ignore dependency if on different cores */
		}
	}

	/* we shouldn't block while holding a component lock */
	if (unlikely(0 != thd->ncs_held)) goto warn;
	if (unlikely(!(thd->blocking_component == 0 ||
		       thd->blocking_component == spdid))) goto warn;
	assert(!sched_thd_free(thd));
	assert(!sched_thd_blocked(thd));

	/*
	 * possible FIXME: should we be modifying the wake_cnt at all
	 * if we are using dependencies?
	 */
	fp_pre_block(thd);

	/* if we already got a wakeup call for this thread */
	if (thd->wake_cnt) {
		assert(thd->wake_cnt == 1);
		cos_sched_lock_release();
		return 0;
	}
	/* dependency thread blocked??? */
	if (dependency_thd) {
		/* printc("thd %d trying to depend on %d\n", thd->id, dependency_thd); */
		dep = sched_get_mapping(dependency_thd);

		if (!dep) {
			printc("Dependency on non-existent thread %d.\n", dependency_thd);
			goto unblock;
		}

		if (dep->dependency_thd) {
			/* circular dependency... */
			if(dep->dependency_thd->id == cos_get_thd_id()) {
				debug_print("BUG @ ");
				printc("cos_sched_base: circular dependency between dep %d, curr %d (From spdid %d)\n", dep->id, cos_get_thd_id(), spdid);
				assert(0);
				goto unblock;
			}
		}

		if (sched_thd_blocked(dep)) {
			printc("dep thd blocked already.\n");
			goto unblock;
		}
	}
	/* dependencies keep the thread on the runqueue, so
	 * that it can be selected to execute and its
	 * dependency list walked. */
	if (dependency_thd) {
		/* printc("...no dependency problem, going to depend on %d\n",dependency_thd); */
		thd->dependency_thd = dep;
		thd->flags |= THD_DEPENDENCY;
		assert(thd->ncs_held == 0);
	} else {
		fp_block(thd, spdid);
	}
	assert(thd->wake_cnt < 2);
	while (0 == thd->wake_cnt) {
		if (dependency_thd) {
			assert(dep && dep == thd->dependency_thd);
			sched_switch_thread_target(0, BLOCK_LOOP, dep);
			cos_sched_lock_take();
			report_event(BLOCKED_W_DEPENDENCY);
			if (!first) { report_event(BLOCKED_DEP_RETRY); }

			/*
			 * Complicated case: We want to avoid the case
			 * where we are dependent on a blocked thread
			 * (i.e. due to self-suspension).  When we
			 * resolve dependencies, if we are dependent
			 * on a blocked thread, we actually run the
			 * dependent thread.  Thus when we wake up
			 * here, we will still be dependent
			 * (otherwise, we would be scheduled because
			 * the dependency thread executed
			 * sched_wakeup).  So in this case, we want to
			 * remove the dependency, execute the thread,
			 * and return an error code from sched_block
			 * indicating that we attempted to do priority
			 * inheritance with a blocked thread.
			 */
			if (unlikely(sched_thd_dependent(thd))) {
				printc("cos_sched_base: Dependency thread self-suspended: dep_thd"
				       " %d, flags %d, wake_cnt %d, spdid %d (curr thd: %d, comp:%d). \n"
				       , dependency_thd, dep->flags, dep->wake_cnt,
				       dep->blocking_component, cos_get_thd_id(), spdid);
				thd->flags &= ~THD_DEPENDENCY;
				thd->dependency_thd = NULL;
				report_event(DEPENDENCY_BLOCKED_THD);
				goto unblock;
			}
		} else {
			sched_switch_thread(0, BLOCK_LOOP);
			cos_sched_lock_take();
			if (!first) { report_event(RETRY_BLOCK); }
		}
		first = 0;
	}
	assert(thd->wake_cnt == 1);
	/* The amount of time we've blocked */
	ret = PERCPU_GET(sched_base_state)->ticks - thd->block_time - 1;
	ret = ret > 0 ? ret : 0;
done:
	assert(thd->wake_cnt == 1);
	assert(!sched_thd_dependent(thd));
	cos_sched_lock_release();
	return ret;
warn:
	printc("Blocking while holding lock!!!\n");
	ret = -1;
	goto done;
unblock:
	fp_pre_wakeup(thd);
	ret = -1;
	goto done;

}

/*
 * component_{take|release} constitute the critical section
 * helping-based wait-free synchronization.  A component can call
 * these functions to take/release the critical section for that
 * component.  Further synchronization primitives can be built up
 * using this in those external components.
 */
int sched_component_take(spdid_t spdid)
{
	struct sched_thd *holder, *curr;
	int first = 1;

	//printc("sched take %d\n", spdid);
	cos_sched_lock_take();
	report_event(COMP_TAKE);
	curr = sched_get_current();
	assert(curr);
	assert(!sched_thd_blocked(curr));
	assert(spdid);

	/* Continue until the critical section is unavailable */
	while (1) {
		report_event(COMP_TAKE_ATTEMPT);
		/* If the current thread is dependent on another thread, switch to it for help! */
		holder = sched_take_crit_sect(spdid, curr);
		if (NULL == holder) break;
		curr->flags |= THD_DEPENDENCY;

		/* FIXME: proper handling of recursive locking */
		assert(curr != holder);
		report_event(COMP_TAKE_CONTENTION);
		sched_switch_thread_target(0, NULL_EVT, holder);
		cos_sched_lock_take();
		if (first) {
			first = 0;
		} else {
			report_event(COMP_TAKE_LOOP);
		}
	}
	curr->flags &= ~THD_DEPENDENCY;
	cos_sched_lock_release();
	assert(!sched_thd_dependent(curr));
	return 0;
}

/* Release the component's lock */
int sched_component_release(spdid_t spdid)
{
	struct sched_thd *curr;

	//printc("sched release %d\n", spdid);

	report_event(COMP_RELEASE);
	cos_sched_lock_take();
	curr = sched_get_current();
	assert(curr);

	if (sched_release_crit_sect(spdid, curr)) {
		printc("fprr: error releasing spd %d's critical section (contended %d)\n",
		       spdid, curr->contended_component);
	}
	sched_switch_thread(0, NULL_EVT);
	assert(!sched_thd_dependent(curr));

	return 0;
}

/* Should not hold scheduler lock when calling. */
static int fp_kill_thd(struct sched_thd *t)
{
	struct sched_thd *c;

	cos_sched_lock_take();
	c = sched_get_current();

	if (!t) printc("kill thread in %d\n", cos_get_thd_id());
	assert(t);
	assert(!sched_thd_grp(t));
	t->flags = THD_DYING;
	sched_set_thd_core(t->id, -1);

	thread_remove(t);

	REM_LIST(t, cevt_next, cevt_prev);
	REM_LIST(t, prio_next, prio_prev);
	REM_LIST(t, next, prev);
	ADD_LIST(&PERCPU_GET(sched_base_state)->graveyard, t, prio_next, prio_prev);
	sched_rem_event(t);

	sched_switch_thread(0, NULL_EVT);

	/* reincarnated!!! */
	assert(!(t->flags & THD_DYING));
	assert(t->spdid != 0);

	if (cos_upcall(COS_UPCALL_THD_CREATE, t->spdid, t->init_data)) prints("fprr: error making upcall into spd.\n");

	if (t == c) {
		printc("t: id %d, c: id %d\n",t->id, c->id);

		BUG();
	}

	return 0;
}

/* returns the @index'th thread id in target,
 * or 0 if no such thread exists. this only works if no threads
 * can enter/exit the target. returning a list would be better, but
 * how to store it? */
int sched_get_thread_in_spd(spdid_t spdid, spdid_t target, int index)
{
	struct sched_thd *t;
	int cnt = 0;
	struct sched_base_per_core *sched_base = PERCPU_GET(sched_base_state);

	/* do we need to worry about child/parent schedulers? */

	/* iterate blocked threads */
	for (t = FIRST_LIST(&sched_base->blocked, prio_next, prio_prev) ;
	     t != &sched_base->blocked ;
	     t = FIRST_LIST(t, prio_next, prio_prev)) {
		if (cos_thd_cntl(COS_THD_INV_SPD, t->id, target, 0) >= 0)
			if (cnt++ == index) return t->id;
	}

	/* do we care about event threads? */

	/* check runqueue */
	t = sched_get_thread_in_spd_from_runqueue(spdid, target, index-cnt);
	if (t) {
		fp_pre_block(t);
		fp_block(t, spdid);
		return t->id;
	}

	return 0;
}

int sched_quarantine_wakeup(spdid_t spdid, int tid)
{
	int ret = 0;
	struct sched_thd *t;

	cos_sched_lock_take();
	t = sched_get_mapping(tid);
	if (!t) goto done;

	fp_pre_wakeup(t);
	fp_wakeup(t, spdid);
done:
	cos_sched_lock_release();
	return ret;
}

int
__sched_quarantine_thread_blocked(spdid_t src, spdid_t dst, struct sched_thd *t)
{
	/* what has to be done if a blocked thread is quarantined?
	 * if the dep thd hasn't been quarantined, then the blk thd should
	 * be woken? So far this is untested code.
	 */
	printc("sched_quarantine: thread %d switch from blocking in %d -> %d\n",
			t->id, t->blocking_component, dst);
	t->blocking_component = dst;
}

int
__sched_quarantine_thread_running(spdid_t src, spdid_t dst, struct sched_thd *t)
{
	/* t is runnable.  */
	int ret = -1;
	struct sched_crit_section *cs;
	cs = &per_core_sched[cos_cpuid()].sched_spd_crit_sections[dst];
	if (cs->holding_thd == t) {
		/* FIXME: what to do about held locks/cs? */
		printc("sched_quarantine: thread %d holds the lock in %d\n", t->id, dst);
		goto done;
	} else if (sched_thd_dependent(t)) {
		/* FIXME: what to do about waiting on another thread? */
		printc("sched_quarantine: thread %d dependent on thread %d\n",
				t->id, t->dependency_thd->id);
		goto done;
	} else {
		/* simple case... right? */
		printc("sched_quarantine: thread %d moving %d -> %d\n", t->id, src, dst);
		if (cos_thd_cntl(COS_THD_INV_SPD, t->id, src, dst) < 0) {
			printc("sched_quarantine: error moving %d from %d -> %d\n", t->id, src, dst);
			goto done;
		}

	}


done:
	return ret;

}

int sched_quarantine_thread(spdid_t spdid, spdid_t src, spdid_t dst, int tid)
{
	int ret = 0;
	struct sched_thd *t;

	cos_sched_lock_take();
	t = sched_get_mapping(tid);
	if (!t) goto done;

	if (t->blocking_component == src) {
		if (cos_thd_cntl(COS_THD_INV_SPD, t->id, src, dst) < 0) {
			printc("sched_quarantine: error fixing inv stk for tid %d from %d -> %d\n", tid, src, dst);
			goto done;
		}
		__sched_quarantine_thread_blocked(src, dst, t);
	} else {
		__sched_quarantine_thread_running(src, dst, t);
	}

done:
	cos_sched_lock_release();
	return ret;
}

/* Create a thread without invoking the scheduler policy */
static struct sched_thd *__sched_setup_thread_no_policy(int tid)
{
	struct sched_thd *new;

	assert(tid > 0);
	new = sched_alloc_thd(tid);
	assert(new);
	if (0 > sched_alloc_event(new)) BUG();
	sched_add_mapping(tid, new);

	return new;
}

extern int parent_sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd);

static struct sched_thd *sched_setup_thread_arg(int dest_spd_id, void *metric_str,
						crt_thd_fn_t fn, void *d, int param)
{
	/* fn and d are function and data pointers if creating within
	 * the scheduler. If creating for a remote component, fn is
	 * the init_data. */
	int tid;
	struct sched_thd *new;

	/* can we reuse an already created (but since killed) thread? */
	if (!EMPTY_LIST(&PERCPU_GET(sched_base_state)->graveyard, prio_next, prio_prev)) {
		/* Only can do remote components here. */
		assert(dest_spd_id && (dest_spd_id != cos_spd_id()));
		new = FIRST_LIST(&PERCPU_GET(sched_base_state)->graveyard, prio_next, prio_prev);
		assert(new->flags & THD_DYING);
		REM_LIST(new, prio_next, prio_prev);
		sched_init_thd(new, new->id, THD_READY);
		new->spdid = (spdid_t)dest_spd_id;
		new->init_data = (int)fn;
		if (0 > sched_alloc_event(new)) BUG();
	} else {
		tid = (sched_is_root())                       ?
			cos_create_thread(dest_spd_id, (int)fn, (int)d) :
			parent_sched_child_thd_crt(cos_spd_id(), (spdid_t)(int)d);
		assert(0 != tid);

		new = __sched_setup_thread_no_policy(tid);
	}
	thread_new(new);

	new->cpuid = cos_cpuid(); /* no thread migration between cores */
	sched_set_thd_core(new->id, cos_cpuid());

	if (param) thread_param_set(new,  (struct sched_param_s *)metric_str);
	else       thread_params_set(new, (char *)metric_str);

	return new;
}

/* this should just create the thread, not set the prio...there should
 * be a separate function for that */
int
sched_create_thread(spdid_t spdid, struct cos_array *data)
{
	(void)spdid; (void)data;
	printc("WARNING: the sched_create_thread function is deprecated.  Please use sched_create_thd\n");
	return -ENOTSUP;
}

#define MAX_NUM_SCHED_PARAM 3

cpuid_t sched_read_param_core_id(u32_t sched_params[MAX_NUM_SCHED_PARAM]) {
	struct sched_param_s param;
	int i;
	cpuid_t core_id = -1;

	for (i = 0; i < MAX_NUM_SCHED_PARAM; i++) {
		param = ((union sched_param)sched_params[i]).c;
		if (param.type == SCHEDP_CORE_ID) {
			core_id = param.value;
			break;
		}
	}

	 /* -1 means not specified. Will create on the current core. */
	if (core_id == -1) core_id = cos_cpuid();

	assert(core_id >= 0 && core_id < NUM_CPU_COS);

	return core_id;
}

/* We may have the upcall thread (on a remote core) to create the new
 * thread. So translate the relative priority to absolute priority
 * before sending the request. */

static int relative_prio_convert(u32_t params[MAX_NUM_SCHED_PARAM]) {
	int i;
	struct sched_param_s param;
	union sched_param u;
	for (i = 0; i < MAX_NUM_SCHED_PARAM; i++) {
		param = ((union sched_param)params[i]).c;
		if (param.type == SCHEDP_RPRIO || param.type == SCHEDP_RLPRIO) {
			/* Translating here. */
			struct sched_thd *c = sched_get_current();
			assert(c);
			unsigned int prio = param.type == SCHEDP_RPRIO ?
				sched_get_metric(c)->priority - param.value:
				sched_get_metric(c)->priority + param.value;
			param.value = prio;
			u.c = param;
			params[i] = u.v;
			break;
		}
	}

	return 0;
}

static inline int sched_create_thd_current_core(u32_t init_data_spdid, u32_t sched_param_0,
						u32_t sched_param_1, u32_t sched_param_2)
{
	struct sched_param_s sp[4];
	struct sched_thd *curr, *new;

	int init_data = init_data_spdid >> 16;
	spdid_t spdid = init_data_spdid & 0xFFFF;

	sp[0] = ((union sched_param)sched_param_0).c;
	sp[1] = ((union sched_param)sched_param_1).c;
	sp[2] = ((union sched_param)sched_param_2).c;
	sp[3] = (union sched_param){.c = {.type = SCHEDP_NOOP}}.c;

	/* printc("Core %ld thd %d going to create new thd...\n", cos_cpuid(), cos_get_thd_id()); */
	cos_sched_lock_take();
	curr = sched_get_current();
	new = sched_setup_thread_arg(spdid, &sp, (void *)init_data, 0, 1);
	cos_sched_lock_release();
	/* printc("Core %ld, sched %d: created thread %d in spdid %d (requested by %d)\n", */
	/*        cos_cpuid(), (unsigned int)cos_spd_id(), new->id, spdid, curr->id); */

	return new->id;
}

int
sched_create_thd(u32_t init_data_spdid, u32_t sched_param_0, u32_t sched_param_1, u32_t sched_param_2)
{
	int ret;
	cpuid_t core;
	u32_t sched_params[MAX_NUM_SCHED_PARAM] = {sched_param_0, sched_param_1, sched_param_2};

	if ((int)(init_data_spdid & 0xFFFF) == cos_spd_id()) return -1;

	core = sched_read_param_core_id(sched_params);
	relative_prio_convert(sched_params);

	if (core != cos_cpuid()) {
		/* Cross-core creation */
		/* printc("xcore create thd, curr thd %d target core %d\n", cos_get_thd_id(), core); */
		if (core < 0 || core >= NUM_CPU_COS) {
			printc("ERROR: Trying to create thread on core %d\n", core);
			goto error;
		}
		u32_t params[4] = {init_data_spdid, sched_params[0], sched_params[1], sched_params[2]};
		ret = xcore_exec(core, sched_create_thd_current_core, 4, params, 1);

		goto done;
	} else {
		ret = sched_create_thd_current_core(init_data_spdid, sched_params[0], sched_params[1], sched_params[2]);
	}
done:
	return ret;
error:
	return 0;
}

#define SCHED_STR_SZ 64

int
sched_thread_params(spdid_t spdid, u16_t thd_id, res_spec_t rs)
{
	struct sched_thd *t;
	int ret = -1;

	cos_sched_lock_take();
	t = sched_get_mapping(thd_id);
	if (!t) goto done;

	ret = thread_resparams_set(t, rs);
done:
	cos_sched_lock_release();
	return ret;
}

static int current_core_create_thread_default(spdid_t spdid, u32_t sched_param_0,
						    u32_t sched_param_1, u32_t sched_param_2)
{
	struct sched_param_s sp[4];
	struct sched_thd *new;

	sp[0] = ((union sched_param)sched_param_0).c;
	sp[1] = ((union sched_param)sched_param_1).c;
	sp[2] = ((union sched_param)sched_param_2).c;
	sp[3] = (union sched_param){.c = {.type = SCHEDP_NOOP}}.c;
	cos_sched_lock_take();
	new = sched_setup_thread_arg(spdid, &sp, 0, 0, 1);
	sched_switch_thread(0, NULL_EVT);
	printc("Core %ld, sched %d: created default thread %d in spdid %d (requested by %d from %d)\n",
	       cos_cpuid(), (unsigned int)cos_spd_id(), new->id, spdid, sched_get_current()->id, spdid);

	if (!new) return -1;

	return 0;
}

/* Create a thread in target with the default parameters */
int
sched_create_thread_default(spdid_t spdid, u32_t sched_param_0,
			    u32_t sched_param_1, u32_t sched_param_2)
{
	int ret;

	u32_t sched_params[MAX_NUM_SCHED_PARAM] = {sched_param_0, sched_param_1, sched_param_2};

	cpuid_t core_id = sched_read_param_core_id(sched_params);

	relative_prio_convert(sched_params);

	if (core_id == cos_cpuid()) {
		ret = current_core_create_thread_default(spdid, sched_params[0], sched_params[1], sched_params[2]);
	} else {
		if (core_id < 0 || core_id >= NUM_CPU_COS) {
			printc("ERROR: Trying to create default thread on core %d\n", core_id);
			return -1;
		}
		u32_t param[4] = {spdid, sched_params[0], sched_params[1], sched_params[2]};
		ret = xcore_exec(core_id, (void *)current_core_create_thread_default, 4, param, 1);
	}

	return 0;
}

static void activate_child_sched(struct sched_thd *g)
{
	assert(sched_thd_grp(g));
	assert(!sched_thd_free(g));
	assert(g->wake_cnt >= 0 && g->wake_cnt <= 2);

	/* if the scheduler's thread is blocked, wake it! */
	if (g->wake_cnt == 0/*sched_thd_blocked(g)*/) {
		assert(sched_thd_blocked(g));
		fp_pre_wakeup(g);
		fp_wakeup(g, 0);
	}
	assert(!sched_thd_blocked(g));
}

/* Only really done when a child scheduler idles */
static void deactivate_child_sched(struct sched_thd *t)
{
	assert(sched_thd_grp(t));
	assert(!sched_thd_free(t));
	assert(sched_thd_ready(t));
	assert(t->wake_cnt > 0 && t->wake_cnt <= 2);

	fp_pre_block(t);
	if (0 == t->wake_cnt) {
		fp_block(t, 0);
		while (0 == t->wake_cnt) {
			assert(sched_thd_blocked(t));
			sched_switch_thread(0, PARENT_CHILD_DEACTIVATE);
			cos_sched_lock_take();
		}
	}

	return;
}

/*
 * Set the calling spd to be the child scheduler of this scheduler,
 * and set the current thread to be the child scheduler's control
 * thread.
 */
int sched_child_cntl_thd(spdid_t spdid)
{
	struct sched_thd *c;

	c = sched_get_current();
	sched_grp_make(c, spdid);
	if (cos_sched_cntl(COS_SCHED_PROMOTE_CHLD, 0, spdid)) BUG();
	if (cos_sched_cntl(COS_SCHED_GRANT_SCHED, c->id, spdid)) BUG();

	c->tick = PERCPU_GET(sched_base_state)->ticks;

	return 0;
}

/*
 * The child scheduler spdid, wishes to create a new thread to be
 * controlled by it in the dest_spd.
 */
int sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd)
{
	struct sched_thd *t, *st, *new;
	int tid;

	cos_sched_lock_take();
	t = sched_get_current();
	if (!(sched_thd_member(t) || sched_thd_grp(t))) goto err;
	st = sched_thd_grp(t) ? t : t->group;
	if (st->cid != spdid) goto err;

	tid = (sched_is_root()) ?
		cos_create_thread(dest_spd, 0, 0) :
		parent_sched_child_thd_crt(cos_spd_id(), dest_spd);

	assert(tid > 0);
	new = sched_alloc_thd(tid);
	assert(new);
	sched_add_mapping(tid, new);
	sched_share_event(new, st);

	sched_grp_add(st, new);
	assert(sched_thd_grp(st) && sched_thd_member(new));
	if (cos_sched_cntl(COS_SCHED_GRANT_SCHED, tid, spdid)) BUG();

//	printc("parent scheduler %d created thread %d for scheduler %d\n",
//	       (unsigned int)cos_spd_id(), tid, spdid);
	cos_sched_lock_release();
	return tid;
err:
	cos_sched_lock_release();
	return -1;
}

static int child_ticks_stale(struct sched_thd *t)
{
	assert(sched_thd_grp(t));
	return t->tick != PERCPU_GET(sched_base_state)->ticks;
}

/* return 1 if the child's time is updated, 0 if not */
static void child_ticks_update(struct sched_thd *t, struct sched_child_evt *e)
{
	u64_t ts, lticks;

	ts = t->tick;
	lticks = PERCPU_GET(sched_base_state)->ticks;
	if (lticks > ts) {
		e->time_elapsed = lticks - ts;
		t->tick = lticks;
	}
}

int sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff)
{
	struct sched_thd *t, *et;
	int c = 0;

	if (!e) return -1;
	e->t            = SCHED_CEVT_OTHER;
	e->tid          = 0;
	e->time_elapsed = 0;

	cos_sched_lock_take();
	t = sched_get_current();
	if (!sched_thd_grp(t) || t->cid != spdid) goto err;
	t->wakeup_tick = t->tick + wake_diff;

	/* While there are no thread events? */
	while (EMPTY_LIST(t, cevt_next, cevt_prev)) {
		/* A acap event has occurred, so send that event OR
		 * the child doesn't want to idle OR the child hasn't
		 * processed timer interrupts.  Regardless, we need to
		 * return to the child. */
		if (t->cevt_flags & SCHED_CEVT_OTHER ||
		    child_ticks_stale(t)             ||
		    !idle) {
			t->cevt_flags &= ~SCHED_CEVT_OTHER;
			child_ticks_update(t, e);
			if (!idle) { report_event(PARENT_CHILD_EVT_OTHER); }
			goto done;
		}
		report_event(PARENT_BLOCK_CHILD);

		/* No event, and child wants to idle: block! This
		 * releases the scheduler lock, so non-local
		 * data-structures need to be reacquired. */
		deactivate_child_sched(t);
	}
	child_ticks_update(t, e);

	/* We have a thread event (blocking or waking) */
	et = FIRST_LIST(t, cevt_next, cevt_prev);
	REM_LIST(et, cevt_next, cevt_prev);
	/* If there are more events, specify that event requests
	 * should continue being made. */
	if (!EMPTY_LIST(t, cevt_next, cevt_prev)) c = 1;

	assert(sched_thd_member(et));
	e->tid = et->id;
	if (sched_thd_blocked(et)) {
		e->t = SCHED_CEVT_BLOCK;
	} else if (sched_thd_ready(et) || sched_thd_dependent(et)) {
		e->t = SCHED_CEVT_WAKE;
	} else {
		printc("child thd evt with flags %x.", et->flags);
		BUG();
	}
	report_event(PARENT_CHILD_EVT_THD);
done:
	t->cevt_flags = 0;
	cos_sched_lock_release();
	return c;
err:
	c = -1;
	goto done;
}

static void sched_process_cevt(struct sched_child_evt *e)
{
	struct sched_thd *t;

	switch(e->t) {
	case SCHED_CEVT_WAKE:
		t = sched_get_mapping(e->tid);
		assert(NULL != t);
		assert(0 == t->wake_cnt);
		if (sched_thd_blocked(t)) {
			fp_pre_wakeup(t);
			fp_wakeup(t, 0);
		}
		report_event(CHILD_EVT_WAKE);
		break;
	case SCHED_CEVT_BLOCK:
		t = sched_get_mapping(e->tid);
		assert(NULL != t);
		fp_pre_block(t);
		assert(0 == t->wake_cnt);
		fp_block(t, 0);
		report_event(CHILD_EVT_BLOCK);
		break;
	case SCHED_CEVT_OTHER:
		report_event(CHILD_EVT_OTHER);
		break;
	}

	/* Process timer ticks which might be 0.  Tick information
	 * can be sent with any event. */
	if (e->time_elapsed) {
		u64_t te = e->time_elapsed;
		static u64_t prev_print = 0;

		PERCPU_GET(sched_base_state)->ticks += te;
		timer_tick(te);

		report_event(TIMER_TICK);
		if ((PERCPU_GET(sched_base_state)->ticks - prev_print) >= (CHLD_REPORT_FREQ*TIMER_FREQ)) {
			report_thd_accouting();
			prev_print += CHLD_REPORT_FREQ*TIMER_FREQ;
		}
		sched_process_wakeups();
	}
}

extern int
parent_sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff);
/*
 * The thread executing in this function (the timer thread in a child
 * scheduler) is invoked in two situations: 1) when the child is idle,
 * and 2) when the parent wishes to run the child and possibly convey
 * to it events such as thread blocking or waking.
 */
static void
sched_child_evt_thd(void)
{
	printc("WARNING: the sched_child_evt_thd function is deprecated.\n");
	return;
#ifdef NIL
	struct sched_child_evt *e;

	e = cos_argreg_alloc(sizeof(struct sched_child_evt));
	assert(NULL != e);
	while (1) {
		int cont, should_idle;

		cos_sched_lock_take();
		do {
			struct sched_thd *n = schedule(NULL);
			unsigned long wake_diff = 0;

			assert(n != PERCPU_GET(sched_base_state)->timer);
			/* If there isn't a thread to schedule, and
			 * there are no events, this child scheduler
			 * should idle */
			should_idle = (n == PERCPU_GET(sched_base_state)->idle);
			if (should_idle) {
				/* locally cache the volatile value */
				u64_t wake_tm, wt, cwt;

				/* check for new timeouts */
				sched_process_wakeups();

				wt = PERCPU_GET(sched_base_state)->wakeup_time;
				cwt = PERCPU_GET(sched_base_state)->child_wakeup_time;
				if (cwt == 0)     wake_tm = wt;
				else if (wt == 0) wake_tm = cwt;
				else              wake_tm = (wt < cwt) ? wt : cwt;

				assert(!wake_tm || wake_tm >= PERCPU_GET(sched_base_state)->ticks);
				wake_diff = wake_tm ? (unsigned long)(wake_tm - PERCPU_GET(sched_base_state)->ticks) : 0;
			}
			cos_sched_clear_cevts();
			cos_sched_lock_release();
			/* Get events from the parent scheduler */
			cont = parent_sched_child_get_evt(cos_spd_id(), e, should_idle, wake_diff);
			cos_sched_lock_take();
			assert(0 <= cont);
			/* Process those events */
			sched_process_cevt(e);
			report_event(should_idle ? CHILD_PROCESS_EVT_IDLE : CHILD_PROCESS_EVT_PEND);
		} while (cont);

		report_event(CHILD_SWITCH_THD);
		/* When there are no more events, schedule */
		sched_switch_thread(0, NULL_EVT);
		assert(EMPTY_LIST(PERCPU_GET(sched_base_state)->timer, prio_next, prio_prev));
	} /* no return */
	cos_argreg_free(e);
#endif
}

int sched_priority(unsigned short int tid)
{
	struct sched_thd *t;
	int p;

	cos_sched_lock_take();
	t = sched_get_mapping(tid);
	if (!t) {
		cos_sched_lock_release();
		return -1;
	}
	p = t->metric.priority;
	cos_sched_lock_release();

	return p;
}

int sched_curr_set_priority(unsigned short int prio)
{
	struct sched_thd *t;
	int p;

	cos_sched_lock_take();
	t = sched_get_current();
	thread_block(t);
	p = t->metric.priority;
	t->metric.priority = prio;
	thread_wakeup(t);
	cos_sched_lock_release();
	return p;
}

unsigned long sched_timestamp(void)
{
	return (unsigned long)PERCPU_GET(sched_base_state)->ticks;
}

int sched_create_net_acap(spdid_t spdid, int acap_id, unsigned short int port)
{
	/* Called by the netif component to link the acap to HW_NET. */

	if (acap_id <= 0) return -1;
	/* This is done by the scheduler, not the netif
	 * component. Only the scheduler should be able to make
	 * acap_wire call. */
	if (cos_acap_wire(spdid << 16 | acap_id, COS_HW_NET, port)) BUG();

	return 0;
}

extern void parent_sched_exit(void);
void sched_exit(void)
{
	/* printc("In sched_exit, core %ld, switching to %d\n", cos_cpuid(), PERCPU_GET(sched_base_state)->init->id); */
	cos_sched_clear_events();
//	cos_switch_thread_release(PERCPU_GET(sched_base_state)->init->id, 0);
	while (1) {
		cos_sched_clear_events();
		cos_switch_thread(PERCPU_GET(sched_base_state)->init->id, 0);
	}
	BUG();
}

static struct sched_thd *fp_create_timer(void)
{
	struct sched_thd *timer_thd;
	union sched_param sp[2] = {{.c = {.type = SCHEDP_TIMER}},
				   {.c = {.type = SCHEDP_NOOP}}};

	assert(sched_is_root());
	timer_thd = sched_setup_thread_arg(cos_spd_id(), &sp, fp_timer, 0, 1);
	if (!timer_thd) BUG();
	PERCPU_GET(sched_base_state)->timer = timer_thd;

	return timer_thd;
}

inline int sched_curr_is_IPI_handler(void) {
	return (sched_get_current() == PERCPU_GET(sched_base_state)->IPI_handler);
}

static inline int exec_fn(int (*fn)(), int nparams, u32_t *params) {
	int ret = -1;

	assert(fn);

	switch (nparams)
	{
	case 0:
		ret = fn();
		break;
	case 1:
		ret = fn(params[0]);
		break;
	case 2:
		ret = fn(params[0], params[1]);
		break;
	case 3:
		ret = fn(params[0], params[1], params[2]);
		break;
	case 4:
		ret = fn(params[0], params[1], params[2], params[3]);
		break;
	}

	return ret;
}

static void IPI_handler(void *d)
{
	struct sched_base_per_core *sched_state = PERCPU_GET(sched_base_state);
	struct xcore_ring_item ring_item;
	struct xcore_fn_data *data;
	int cli_acap, srv_acap, ret;
	struct ck_ring *ring = PERCPU_GET(percpu_ring);
	void *ring_buf = PERCPU_GET(xcore_ring_page);

	ret = cos_async_cap_cntl(COS_ACAP_CREATE, cos_spd_id(), cos_spd_id(), cos_get_thd_id());

	cli_acap = ret >> 16;
	srv_acap = ret & 0xFFFF;

	if (unlikely(cli_acap <= 0 || srv_acap <= 0)) {
		printc("Scheduler %ld could not allocate acap for IPI handler %d.\n", cos_spd_id(), cos_get_thd_id());
		BUG();
	}
	sched_state->IPI_acap = cli_acap;

	if (ret < 0) BUG();

	ck_ring_init(ring, leqpow2(PAGE_SIZE / sizeof(struct xcore_ring_item)));

	printc("Core %ld: Starting IPI handling thread (thread id %d, client acap %d, server acap %d)\n",
	       cos_cpuid(), cos_get_thd_id(), cli_acap, srv_acap);
	while (1) {
		cos_sched_lock_take();
		/* Going to switch away */
		/* Do not use areceive syscall here. We are in the scheduler. */
		sched_switch_thread(COS_SCHED_ACAP_WAIT, EVT_CMPLETE_LOOP);

		/* Received an IPI! */
		while (CK_RING_DEQUEUE_SPSC(xcore_ring, ring, (struct xcore_ring_item *)ring_buf, &ring_item) == true) {
			data = ring_item.data;
			assert(data->nparams <= 4);
			assert(data->finished == 0);
			assert(data->fn);
			//printc("core %d executing...\n", cos_cpuid());
			data->ret = exec_fn(data->fn, data->nparams, data->params);
			//printc("core %d done exe.\n", cos_cpuid());
			data->finished = 1;
		}
		/* printc("Core %ld: done processing IPI request!\n", cos_cpuid()); */
	}

	BUG();
}

static struct sched_thd *fp_create_IPI_handler(void)
{
	union sched_param sp[2] = {{.c = {.type = SCHEDP_IPI_HANDLER}},
				   {.c = {.type = SCHEDP_NOOP}}};
	struct sched_base_per_core *sched_state = PERCPU_GET(sched_base_state);

	assert(sched_is_root());

	int spdid = cos_spd_id();

	sched_state->IPI_handler = sched_setup_thread_arg(spdid, &sp, IPI_handler, 0, 1);
	if (sched_state->IPI_handler == NULL) BUG();

	printc("Core %ld: IPI handling thread created (thread id %d).\n",
	       cos_cpuid(), sched_state->IPI_handler->id);

	return sched_state->IPI_handler;
}

/* Iterate through the configuration and create threads for
 * components as appropriate */
static void
sched_init_create_threads(int boot_threads)
{
	struct sched_thd *t;
	union sched_param sp[4] = {{.c = {.type = SCHEDP_IDLE}},
				   {.c = {.type = SCHEDP_NOOP}},
				   {.c = {.type = SCHEDP_NOOP}},
				   {.c = {.type = SCHEDP_NOOP}}};

	/* create the idle thread */
	PERCPU_GET(sched_base_state)->idle = sched_setup_thread_arg(cos_spd_id(), &sp, fp_idle_loop, 0, 1);

	printc("Core %ld: Idle thread has id %d with priority %s.\n", cos_cpuid(), PERCPU_GET(sched_base_state)->idle->id, "i");

	if (!boot_threads) return;

	sp[0].c.type = SCHEDP_INIT;
	t = sched_setup_thread_arg(BOOT_SPD, &sp, 0, 0, 1);
	assert(t);
	printc("Initialization thread has id %d.\n", t->id);
}

static void sched_init_thd_core_mapping(void)
{
	int i, ret;
	for (i = 0; i < SCHED_NUM_THREADS; i++) {
		ret = sched_set_thd_core(i, -1);
		assert(ret == 0);
	}

	return;
}

/* Initialize data-structures */
static void
__sched_init(void)
{
	/* Should be done for each core. */
	struct sched_base_per_core *sched_state = PERCPU_GET(sched_base_state);
	sched_init_thd(&sched_state->blocked, 0, THD_FREE);
	sched_init_thd(&sched_state->upcall_deactive, 0, THD_FREE);
	sched_init_thd(&sched_state->graveyard, 0, THD_FREE);
	sched_ds_init();
	sched_initialization();
	sched_init_thd_core_mapping();

	return;
}

extern int
parent_sched_child_cntl_thd(spdid_t spdid);

/* Initialize the child scheduler. */
static void
sched_child_init(void)
{
	/* Child scheduler */
	sched_type = SCHED_CHILD;
	__sched_init();

	/* Don't involve the scheduler policy... */
	PERCPU_GET(sched_base_state)->timer = __sched_setup_thread_no_policy(cos_get_thd_id());
	assert(PERCPU_GET(sched_base_state)->timer);
	sched_set_thd_urgency(PERCPU_GET(sched_base_state)->timer, 0); /* highest urgency */
	PERCPU_GET(sched_base_state)->timer->flags |= THD_PHANTOM;

	sched_init_create_threads(0);

	sched_child_evt_thd();	/* doesn't return */

	return;
}

static void
print_config_info(void)
{
	printc("Please ensure that the following information is correct.\n");
	printc("If it is not, edit kernel/include/shared/cos_config.h.\n");
	printc("CPU_FREQUENCY=%lld\n"
	       "TIMER_FREQUENCY=%lld\n",
	       (unsigned long long)CPU_FREQUENCY,
	       (unsigned long long)TIMER_FREQ);
	printc("USEC_PER_TICK=%lld\n"
	       "CYC_PER_USEC=%lld\n",
	       (unsigned long long)USEC_PER_TICK,
	       (unsigned long long)CYC_PER_USEC);
	printc("Total number of CPUs: %d. Composite runs on core 0 - %d. ", NUM_CPU, (NUM_CPU - 2) >= 0 ? (NUM_CPU - 2) : 0);
	printc("Linux runs on core %d.\n", NUM_CPU - 1);
}

extern struct cos_component_information cos_comp_info;
#define NREGIONS 4

/* Initialize the root scheduler */
volatile int initialized[NUM_CPU_COS];
int sched_root_init(void)
{
	struct sched_thd *new;
	int ret, i;

	if (cos_cpuid() == INIT_CORE) {
		assert(initialized[INIT_CORE] == 0);
		print_config_info();
		/* Init the cross-core communicate spin lock. */
	} else {
		while (initialized[INIT_CORE] == 0) ;
	}

	/* printc("<<< CPU %ld, in root init, thd %d going to run.>>>\n", cos_cpuid(), cos_get_thd_id()); */

	__sched_init();

	/* switch back to this thread to terminate the system. */
	PERCPU_GET(sched_base_state)->init = sched_alloc_thd(cos_get_thd_id());
	assert(PERCPU_GET(sched_base_state)->init);

	sched_init_create_threads(initialized[INIT_CORE] == 0);

	/* Create the clock tick (timer) thread */
	fp_create_timer();

	/* Create the IPI handling thread for the current core */
	fp_create_IPI_handler();

	new = schedule(NULL);

	initialized[cos_cpuid()] = 1;
	for (i = 0; i < NUM_CPU_COS; i++) {
		/* Sync to start */
		while (initialized[i] == 0) ;
	}
	printc("<<<Core %ld, thread %d: sched_init done.>>>\n", cos_cpuid(), cos_get_thd_id());

	if ((ret = cos_switch_thread(new->id, 0))) {
		printc("switch thread failed with %d\n", ret);
	}

	printc("<<<Core %ld, thread %d: Exiting system.>>>\n", cos_cpuid(), cos_get_thd_id());
	parent_sched_exit();
	assert(0);

	return 0;
}

int
sched_isroot(void) { return 0; }

extern int parent_sched_isroot(void);
int
sched_init(void)
{
	/* printc("Sched init has thread %d, spd %d\n", cos_get_thd_id(), cos_spd_id()); */
	assert(!(PERCPU_GET(sched_base_state)->init)); // don't re-initialize. should be removed if doing recovery test.

	/* Promote us to a scheduler! */
	if (parent_sched_child_cntl_thd(cos_spd_id())) BUG();

	if (cos_sched_cntl(COS_SCHED_EVT_REGION, 0, (long)PERCPU_GET(cos_sched_notifications))) BUG();

	/* Are we root? */
	if (parent_sched_isroot()) sched_root_init();
	else                       sched_child_init();
	return 0;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	/* printc("upcall type %d, core %ld, thd %d, args %p %p %p\n", */
	/*        t, cos_cpuid(), cos_get_thd_id(), arg1, arg2, arg3); */
	switch (t) {
	case COS_UPCALL_THD_CREATE:
		/* cos_upcall and cos_create_thread syscall pass this type. */
		if (!arg1)  {
			/* The llboot upcalls into here with arg1 = NULL */
			sched_init();
			break;
		} else {
			((crt_thd_fn_t)arg1)(arg2);
		}
		break;
	case COS_UPCALL_DESTROY:
		fp_kill_thd(sched_get_current());
		break;
	case COS_UPCALL_UNHANDLED_FAULT:
		printc("Unhandled fault occurred, exiting system\n");
		sched_exit();
		break;
	case COS_UPCALL_ACAP_COMPLETE:
		fp_event_completion(sched_get_current());
		break;
	case COS_UPCALL_QUARANTINE:
		break;
	default:
		printc("fp_rr: cos_upcall_fn error - type %x, arg1 %d, arg2 %d\n",
		      (unsigned int)t, (unsigned int)arg1, (unsigned int)arg2);
		BUG();
		return;
	}

	return;
}
