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
#include <cos_scheduler.h>
#include <cos_time.h>
#include <print.h>

#include <cos_sched_tk.h>

#include <sched.h>

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

static volatile u64_t ticks = 0;

/* When the wakeup thread (in charge of higher-level timing) should be woken */
static volatile u64_t wakeup_time = 0;
static volatile u64_t child_wakeup_time = 0;
static struct sched_thd *wakeup_thd;

/* 
 * timer is the timer tick thread for a root scheduler, or the child
 * event thread for a child scheduler.  init is the initial bootup
 * thread for a root scheduler.  init is meaningless for a child
 * scheduler.  idle is the idle thread, again only meaningful for root
 * schedulers.
 */
static struct sched_thd *timer, *init, *idle;
static struct sched_thd blocked;
static struct sched_thd upcall_deactive;
static struct sched_thd graveyard;

static enum {SCHED_CHILD, SCHED_ROOT} sched_type = SCHED_ROOT;
static inline int sched_is_root(void) { return sched_type == SCHED_ROOT; }
static inline int sched_is_child(void) { return !sched_is_root(); } 

//#define FPRR_REPORT_EVTS

typedef enum {
	NULL_EVT = 0,
	SWITCH_THD,
	BRAND_ACTIVE,
	BRAND_READY,
	BRAND_PENDING,
	BRAND_CYCLE,
	SCHED_DEPENDENCY,
	THD_BLOCK,
	THD_WAKE,
	COMP_TAKE,
	COMP_TAKE_ATTEMPT,
	COMP_TAKE_CONTENTION,
	COMP_RELEASE,
	TIMER_TICK,
	TIMER_SWITCH_LOOP,
	IDLE_SCHED,
	IDLE_SCHED_SWITCH,
	EVT_CMPLETE,
	BLOCK_LOOP,
	WAKE_LOOP,
	TIMER_LOOP,
	COMP_TAKE_LOOP,
	EVT_CMPLETE_LOOP,
	TIMEOUT_LOOP,
	IDLE_SCHED_LOOP,
	BLOCKED_DEP_RETRY,
	RETRY_BLOCK,
	BLOCKED_W_DEPENDENCY,
	DEPENDENCY_BLOCKED_THD,
	SCHED_TARGETTED_DEPENDENCY,
	PARENT_BLOCK_CHILD,
	PARENT_CHILD_EVT_OTHER,
	PARENT_CHILD_EVT_THD,
	PARENT_CHILD_DEACTIVATE,
	PARENT_CHILD_RESUME,
	PARENT_CHILD_REDUNDANT_RESUME,
	CHILD_PROCESS_EVT_IDLE,
	CHILD_PROCESS_EVT_PEND,
	CHILD_EVT_BLOCK,
	CHILD_EVT_WAKE,
	CHILD_EVT_OTHER,
	CHILD_SWITCH_THD,
	CEVT_RESCHED,
	REVT_LAST
} report_evt_t;

#ifdef FPRR_REPORT_EVTS

static char *revt_names[] = {
	"null event",
	"switch threads",
	"brand active event",
	"brand ready event",
	"brand pending event",
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
static long long report_evts[REVT_LAST];

/* Timers */
enum {
	TIMER_SCHED = 0,
	TIMER_FPRR,
	TIMER_SWTCH,
	TIMER_MAX
};
STATIC_TIMER_RECORDS(recs, TIMER_MAX);

static void report_event(report_evt_t evt)
{
	if (unlikely(evt >= REVT_LAST)) return;

	report_evts[evt]++;
}

static void report_output(void)
{
	int i;

	prints("All counters:\n");
	for (i = 0 ; i < REVT_LAST ; i++) {
		printc("\t%s: %lld\n", revt_names[i], report_evts[i]);
		report_evts[i] = 0;
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

	runqueue_print();

	if (sched_is_child()) {
		struct sched_accounting *sa = sched_get_accounting(timer);

		printc("\nChild timer thread (thd, ticks):\n");		
		printc("\t%d, %ld\n", timer->id, sa->ticks - sa->prev_ticks);
		sa->prev_ticks = sa->ticks;
	}

	printc("\nBlocked threads (thd, prio, ticks):\n");
	for (t = FIRST_LIST(&blocked, prio_next, prio_prev) ; 
	     t != &blocked ;
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
	for (t = FIRST_LIST(&upcall_deactive, prio_next, prio_prev) ; 
	     t != &upcall_deactive ;
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
		assert(sched_is_root() || (t != timer && !sched_thd_phantom(t)));
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
		assert(sched_is_root() || (uc != timer && !sched_thd_phantom(uc)));
		thread_wakeup(uc);
	}
}

static void fp_deactivate_upcall(struct sched_thd *uc)
{
	uc->flags &= ~THD_READY;
	uc->flags |= THD_UC_READY;
	thread_block(uc);
	assert(EMPTY_LIST(uc, prio_next, prio_prev));
	ADD_LIST(&upcall_deactive, uc, prio_next, prio_prev);
}

/* scheduler lock should already be taken */
static void evt_callback(struct sched_thd *t, u8_t flags, u32_t cpu_usage)
{
	assert(!sched_thd_member(t));

	if (sched_thd_free(t) || sched_thd_dying(t)) return;

	if (flags & (COS_SCHED_EVT_BRAND_ACTIVE|COS_SCHED_EVT_BRAND_READY|COS_SCHED_EVT_BRAND_PEND)) {
		if (sched_thd_grp(t)) {
			activate_child_sched(t);
			t->cevt_flags |= SCHED_CEVT_OTHER;
		} else if (flags & COS_SCHED_EVT_BRAND_ACTIVE) {
			report_event(BRAND_ACTIVE);
			fp_activate_upcall(t);
		} else if (flags & COS_SCHED_EVT_BRAND_READY) {
			assert(sched_get_current() != t);
			report_event(BRAND_READY);
			fp_deactivate_upcall(t);
		} else if (flags & COS_SCHED_EVT_BRAND_PEND) {
			BUG();
		}
	} else {
		report_event(BRAND_CYCLE);
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
		assert(!(next->flags & (COS_SCHED_BRAND_WAIT|COS_SCHED_TAILCALL)));
		assert(!sched_thd_blocked(dep) && sched_thd_ready(dep));
		assert(!sched_thd_free(dep));
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
 * cos_switch_thread, and if they include tailcall, or brand wait,
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
		
		if (likely(!target)) {
			/* 
			 * If current is an upcall that wishes to terminate
			 * its execution upon switching to the next thread,
			 * then it will pass in one of the following flags.
			 * If that is the case, then we wish to ask the
			 * scheduler to return a thread that is _not_, even if
			 * current is the highest priority thread because
			 * current will terminate execution with the switch.
			 */
			if (flags & (COS_SCHED_BRAND_WAIT|COS_SCHED_TAILCALL)) {
				/* we don't want next to be us! We are an
				 * upcall completing execution */
				timer_start(&tfp);
				next = schedule(current);
				timer_end(&tfp);
				assert(sched_is_root() || timer != next);
				assert(next != current);
				assert(!sched_thd_member(next));
			} else {
				timer_start(&tfp);
				next = schedule(NULL);
				timer_end(&tfp);
				assert(sched_is_root() || timer != next);
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

		if (sched_is_child() && unlikely(next == idle)) {
			/* This is a kludge: child schedulers don't
			 * have idle threads...instead they just use
			 * the event/timer thread */
			/* We are in the timer/child event thread! */
			if (current == timer) goto done;
			next = timer;
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
	if (wakeup_time && ticks >= wakeup_time && likely(wakeup_thd)) {
		if (wakeup_thd->wake_cnt < 2) {
			wakeup_time = 0;
			fp_pre_wakeup(wakeup_thd);
			/* If the event thread has not blocked yet, then don't
			 * wake it fully */
			if (sched_thd_blocked(wakeup_thd)) fp_wakeup(wakeup_thd, 0);
		} else {
			assert(!sched_thd_blocked(wakeup_thd));
		}
	}

	/* 
	 * Find child scheduler threads and monitor for timeouts
	 * FIXME: should organize child schedulers into sorted wakeup
	 * list instead of using a linear walk through all blocked
	 * threads here.
	 */
	for (t = FIRST_LIST(&blocked, prio_next, prio_prev) ;
	     t != &blocked ;
	     t = next) {
		next = FIRST_LIST(t, prio_next, prio_prev);
		/* child scheduler requested wakeup */
		if (sched_thd_grp(t) && t->wakeup_tick) {
			if (t->wakeup_tick <= ticks) {
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
	child_wakeup_time = lowest_child;
}

static void sched_timer_tick(void)
{
	while (1) {
		cos_sched_lock_take();
		
		report_event(TIMER_TICK);
		
		if (unlikely(ticks % (REPORT_FREQ*TIMER_FREQ)) == ((REPORT_FREQ*TIMER_FREQ)-1)) {
			report_thd_accouting();
			//cos_stats();
		}
		
		/* are we done running? */
<<<<<<< HEAD
		if (ticks >= RUNTIME_SEC*TIMER_FREQ+1) {
=======
		if (unlikely(ticks >= RUNTIME_SEC*TIMER_FREQ+1)) {
>>>>>>> origin/master
			while (COS_SCHED_RET_SUCCESS !=
			       cos_switch_thread_release(init->id, COS_SCHED_BRAND_WAIT)) {
				cos_sched_lock_take();
				if (cos_sched_pending_event()) {
					cos_sched_clear_events();
					cos_sched_process_events(evt_callback, 0);
				}
			}
		}
		
		ticks++;
		sched_process_wakeups();

		timer_tick(1);
		sched_switch_thread(COS_SCHED_BRAND_WAIT, TIMER_SWITCH_LOOP);
		/* Tailcall out of the loop */
	}
}

static void fp_event_completion(struct sched_thd *e)
{
	report_event(EVT_CMPLETE);

	cos_sched_lock_take();
	sched_switch_thread(COS_SCHED_TAILCALL, EVT_CMPLETE_LOOP);
	BUG();

	return;
}

/* type of newly created thread functions */
typedef void (*crt_thd_fn_t)(void *data);

static void fp_timer(void *d)
{
	printc("Starting timer\n");
	sched_timer_tick();
	BUG();
}

static void fp_create_spd_thd(void *d)
{
	int spdid = (int)d;

	if (cos_upcall(spdid)) {
		prints("fprr: error making upcall into spd.\n");
	}
	BUG();
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
		report_event(IDLE_SCHED_LOOP);
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

	abs_timeout = ticks + amnt;

	if (0 == wakeup_time || abs_timeout < wakeup_time) {
		wakeup_time = abs_timeout;
	}
	/* If we're the timer thread, lets block, otherwise return */
	if (thd != wakeup_thd) {
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
	wakeup_thd = thd;

	cos_sched_lock_release();
	return 0;
}

/* increment the wake count and do sanity checking*/
static void fp_pre_wakeup(struct sched_thd *t)
{
	assert(t->wake_cnt >= 0 && t->wake_cnt <= 2);
	if (2 == t->wake_cnt) return;
	t->wake_cnt++;
	//printc("thd %d cnt++ %d ->%d\n", t->id, t->wake_cnt-1, t->wake_cnt);
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
	struct sched_thd *thd;
	
	cos_sched_lock_take();
		
	//printc("thread %d waking up thread %d. %d", cos_get_thd_id(), thd_id, 0);
	
	thd = sched_get_mapping(thd_id);
	if (!thd) goto error;
	
	/* only increase the count once */
	fp_pre_wakeup(thd);
	
	if (thd->dependency_thd) {
		assert(sched_thd_dependent(thd));
		assert(thd->ncs_held == 0 || thd->id == cos_get_thd_id());
		thd->flags &= ~THD_DEPENDENCY;
		thd->dependency_thd = NULL;
	} else {
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
	}

	sched_switch_thread(0, WAKE_LOOP);
done:
	return 0;
cleanup:
	cos_sched_lock_release();
	goto done;
error:
	cos_sched_lock_release();
	return -1;
}

/* decrement the wake count, do sanity checking, and record at what
 * time the thread has been woken */
static void fp_pre_block(struct sched_thd *thd)
{
	/* A blocked thread can't block... */
	assert(thd->wake_cnt > 0);
	assert(thd->wake_cnt <= 2);
	thd->wake_cnt--;
	thd->block_time = ticks;
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
	ADD_LIST(&blocked, t, prio_next, prio_prev);
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
int sched_block(spdid_t spdid, unsigned short int dependency_thd)
{
	struct sched_thd *thd, *dep = NULL;
	int ret;
	int first = 1;

	// Added by Gabe 08/19
	if (dependency_thd == cos_get_thd_id()) return -EINVAL;

	cos_sched_lock_take();
	thd = sched_get_current();
	assert(thd);
	assert(spdid);

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
		dep = sched_get_mapping(dependency_thd);
		if (dep->dependency_thd) {
			if(dep->dependency_thd->id == cos_get_thd_id()) {
				printc("dep %d, curr %d\n", dep->id, cos_get_thd_id());
				cos_sched_lock_release();
				assert(0);
			}
		}

		if (!dep) {
			printc("Dependency on non-existent thread %d.\n", dependency_thd);
			goto err;
		}
		if (sched_thd_blocked(dep)) goto err;
	}
	/* dependencies keep the thread on the runqueue, so
	 * that it can be selected to execute and its
	 * dependency list walked. */
	if (dependency_thd) {
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
				printc("Cos_sched_base: Dependency thread self-suspended: dep_thd %d, flags %d, wake_cnt %d (curr thd: %d, comp:%d). \n"
				       , dependency_thd, dep->flags, dep->wake_cnt, cos_get_thd_id(), spdid);
				thd->flags &= ~THD_DEPENDENCY;
				thd->dependency_thd = NULL;
				report_event(DEPENDENCY_BLOCKED_THD);
				goto err;
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
	ret = ticks - thd->block_time - 1;
	ret = ret > 0 ? ret : 0;
	cos_sched_lock_release();
	return ret; 
err:
	cos_sched_lock_release();
	return -1;
warn:
	printc("Blocking while holding lock!!!\n");
	goto err;
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
	cos_sched_lock_release();
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

	return 0;
}

/* Should not hold scheduler lock when calling. */
static int fp_kill_thd(struct sched_thd *t)
{
	struct sched_thd *c; 

	cos_sched_lock_take();
	c = sched_get_current();

	assert(!sched_thd_grp(t));
	t->flags = THD_DYING;

	thread_remove(t);

	REM_LIST(t, cevt_next, cevt_prev);
	REM_LIST(t, prio_next, prio_prev);
	REM_LIST(t, next, prev);
	ADD_LIST(&graveyard, t, prio_next, prio_prev);

	sched_switch_thread(0, NULL_EVT);

	/* reincarnated!!! */
	assert(!(t->flags & THD_DYING));
	assert(t->spdid != 0);
	fp_create_spd_thd((void*)(unsigned int)t->spdid);
	
	if (t == c) {
		printc("t: id %d, c: id %d\n",t->id, c->id);

		BUG();
	}

	return 0;
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

static struct sched_thd *sched_setup_thread_arg(void *metric_str, crt_thd_fn_t fn, void *d, int parm)
{
	int tid;
	struct sched_thd *new;

	/* can we reuse an already created (but since killed) thread? */
	if (!EMPTY_LIST(&graveyard, prio_next, prio_prev)) {
		assert(fn == fp_create_spd_thd); /* can't do the timer here */
		assert(d != NULL);
		new = FIRST_LIST(&graveyard, prio_next, prio_prev);
		assert(new->flags & THD_DYING);
		REM_LIST(new, prio_next, prio_prev);
		sched_init_thd(new, new->id, THD_READY);
		new->spdid = (spdid_t)(unsigned int)d;
	} else {
		tid = (sched_is_root())                       ?
			cos_create_thread((int)fn, (int)d, 0) :
			parent_sched_child_thd_crt(cos_spd_id(), (spdid_t)(int)d);
		assert(0 != tid);

		new = __sched_setup_thread_no_policy(tid);
	}
	thread_new(new);
	if (parm) thread_param_set(new,  (struct sched_param_s *)metric_str);
	else      thread_params_set(new, (char *)metric_str);
	
	return new;
}

/* this should just create the thread, not set the prio...there should
 * be a separate function for that */
int
sched_create_thread(spdid_t spdid, struct cos_array *data)
{
	struct sched_thd *curr, *new;
	void *d = (void*)(int)spdid;
	char *metric_str;

	printc("WARNING: the sched_create_thread function is deprecated.  Please use sched_create_thd\n");
	
	if (!cos_argreg_arr_intern(data)) return -1;
	if (((char *)data->mem)[data->sz-1] != '\0') return -1;

	cos_sched_lock_take();
	curr = sched_get_current();
	metric_str = (char *)data->mem;
	new = sched_setup_thread_arg((char *)metric_str, fp_create_spd_thd, d, 0);
	cos_sched_lock_release();
	printc("sched %d: created thread %d in spdid %d (requested by %d)\n",
	       (unsigned int)cos_spd_id(), new->id, spdid, curr->id);

	return new->id;
}

int
sched_create_thd(spdid_t spdid, u32_t sched_param0, u32_t sched_param1, u32_t sched_param2)
{
	struct sched_param_s sp[4];
	struct sched_thd *curr, *new;
	void *d = (void*)(int)spdid;

	sp[0] = ((union sched_param)sched_param0).c;
	sp[1] = ((union sched_param)sched_param1).c;
	sp[2] = ((union sched_param)sched_param2).c;
	sp[3] = (union sched_param){.c = {.type = SCHEDP_NOOP}}.c;

	cos_sched_lock_take();
	curr = sched_get_current();
	new = sched_setup_thread_arg(&sp, fp_create_spd_thd, d, 1);
	cos_sched_lock_release();
	printc("sched %d: created thread %d in spdid %d (requested by %d)\n",
	       (unsigned int)cos_spd_id(), new->id, spdid, curr->id);

	return new->id;
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

/* Create a thread in target with the default parameters */
int
sched_create_thread_default(spdid_t spdid, u32_t sched_param_0, 
			    u32_t sched_param_1, u32_t sched_param_2)
{
	struct sched_param_s sp[4];
	struct sched_thd *new;
	vaddr_t t = spdid;

	sp[0] = ((union sched_param)sched_param_0).c;
	sp[1] = ((union sched_param)sched_param_1).c;
	sp[2] = ((union sched_param)sched_param_2).c;
	sp[3] = (union sched_param){.c = {.type = SCHEDP_NOOP}}.c;
	
	cos_sched_lock_take();
	new = sched_setup_thread_arg(&sp, fp_create_spd_thd, (void*)t, 1);
	sched_switch_thread(0, NULL_EVT);
	if (!new) return -1;
	printc("sched %d: created default thread %d in spdid %d (requested by %d from %d)\n",
	       (unsigned int)cos_spd_id(), new->id, spdid, sched_get_current()->id, spdid);

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

	c->tick = ticks;

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
		cos_create_thread((int)fp_create_spd_thd, (int)dest_spd, 0) :
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
	return t->tick != ticks;
}

/* return 1 if the child's time is updated, 0 if not */
static void child_ticks_update(struct sched_thd *t, struct sched_child_evt *e)
{
	u64_t ts, lticks;

	ts = t->tick;
	lticks = ticks;
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
		/* A brand event has occurred, so send that event OR
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

		ticks += te;
		timer_tick(te);
		
		report_event(TIMER_TICK);
		if ((ticks - prev_print) >= (CHLD_REPORT_FREQ*TIMER_FREQ)) {
			report_thd_accouting();
			prev_print += CHLD_REPORT_FREQ*TIMER_FREQ;
		}
		sched_process_wakeups();
	}
}

extern int parent_sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff);
/* 
 * The thread executing in this function (the timer thread in a child
 * scheduler) is invoked in two situations: 1) when the child is idle,
 * and 2) when the parent wishes to run the child and possibly convey
 * to it events such as thread blocking or waking.
 */
static void sched_child_evt_thd(void)
{
	struct sched_child_evt *e;

	e = cos_argreg_alloc(sizeof(struct sched_child_evt));
	assert(NULL != e);
	while (1) {
		int cont, should_idle;

		cos_sched_lock_take();
		do {
			struct sched_thd *n = schedule(NULL);
			unsigned long wake_diff = 0;

			assert(n != timer);
			/* If there isn't a thread to schedule, and
			 * there are no events, this child scheduler
			 * should idle */
			should_idle = (n == idle);
			if (should_idle) {
				/* locally cache the volatile value */
				u64_t wake_tm, wt, cwt;

				/* check for new timeouts */
				sched_process_wakeups();

				wt = wakeup_time;
				cwt = child_wakeup_time;
				if (cwt == 0)     wake_tm = wt;
				else if (wt == 0) wake_tm = cwt;
				else              wake_tm = (wt < cwt) ? wt : cwt;

				assert(!wake_tm || wake_tm >= ticks);
				wake_diff = wake_tm ? (unsigned long)(wake_tm - ticks) : 0;
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
		assert(EMPTY_LIST(timer, prio_next, prio_prev));
	} /* no return */
	cos_argreg_free(e);
}

/* return the id of the brand created */
static int sched_setup_brand(spdid_t spdid)
{
	unsigned short int b_id;

	b_id = cos_brand_cntl(COS_BRAND_CREATE_HW, 0, 0, spdid);

	return b_id;
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

unsigned long sched_timestamp(void)
{
	return (unsigned long)ticks;
}

int sched_create_net_brand(spdid_t spdid, unsigned short int port)
{
	int b_id;

	b_id = sched_setup_brand(spdid);
	assert(b_id >= 0);
	if (0 > cos_brand_wire(b_id, COS_HW_NET, port)) BUG();

	return b_id;
}

int sched_add_thd_to_brand(spdid_t spdid, unsigned short int bid, unsigned short int tid)
{
	struct sched_thd *t;
	int ret;

	t = sched_get_mapping(tid);
	if (NULL == t) return -1;
	ret = cos_brand_cntl(COS_BRAND_ADD_THD, bid, tid, 0);
	if (0 > ret) return -1;

	return 0;
}

void sched_exit(void)
{
	
	printc("Switching to %d\n", init->id);
	cos_sched_clear_events();
	cos_switch_thread(init->id, 0);
	while (1) ;
	BUG();
}

static struct sched_thd *fp_create_timer(void)
{
	int bid;
	union sched_param sp[2] = {{.c = {.type = SCHEDP_TIMER}},
				   {.c = {.type = SCHEDP_NOOP}}};

	bid = sched_setup_brand(cos_spd_id());
	assert(sched_is_root());
	timer = sched_setup_thread_arg(&sp, fp_timer, (void*)bid, 1);
	if (NULL == timer) BUG();
	if (0 > sched_add_thd_to_brand(cos_spd_id(), bid, timer->id)) BUG();
	printc("Timer thread has id %d with priority %s.\n", timer->id, "t");
	cos_brand_wire(bid, COS_HW_TIMER, 0);

	return timer;
}

/* Iterate through the configuration and create threads for
 * components as appropriate */
static void sched_init_create_threads(int boot_threads)
{
	struct sched_thd *t;
	union sched_param sp[4] = {{.c = {.type = SCHEDP_IDLE}}, 
				   {.c = {.type = SCHEDP_NOOP}}, 
				   {.c = {.type = SCHEDP_NOOP}}, 
				   {.c = {.type = SCHEDP_NOOP}}};

	/* create the idle thread */
	idle = sched_setup_thread_arg(&sp, fp_idle_loop, NULL, 1);
	printc("Idle thread has id %d with priority %s.\n", idle->id, "i");

	if (!boot_threads) return;

	sp[0].c.type = SCHEDP_INIT;
	t = sched_setup_thread_arg(&sp, fp_create_spd_thd, (void*)(int)3, 1);	
	assert(t);
	printc("Initialization thread has id %d.\n", t->id);
	/* t = sched_setup_thread_arg(&sp, fp_create_spd_thd, (void*)(int)6, 1);	 */
	/* assert(t); */
	/* printc("Initialization thread has id %d.\n", t->id); */
}

/* Initialize data-structures */
static void sched_init(void)
{
	static int first = 1;

	assert(first);
	first = 0;

	sched_init_thd(&blocked, 0, THD_FREE);
	sched_init_thd(&upcall_deactive, 0, THD_FREE);
	sched_init_thd(&graveyard, 0, THD_FREE);
	sched_ds_init();
	sched_initialization();

	return;
}

extern int parent_sched_child_cntl_thd(spdid_t spdid);

/* Initialize the child scheduler. */
static void sched_child_init(void)
{
	/* Child scheduler */
	sched_type = SCHED_CHILD;
	if (parent_sched_child_cntl_thd(cos_spd_id())) BUG();
	if (cos_sched_cntl(COS_SCHED_EVT_REGION, 0, (long)&cos_sched_notifications)) BUG();
	sched_init();

	/* Don't involve the scheduler policy... */
	timer = __sched_setup_thread_no_policy(cos_get_thd_id());
	assert(timer);
	sched_set_thd_urgency(timer, 0); /* highest urgency */
	timer->flags |= THD_PHANTOM;

	sched_init_create_threads(0);

	sched_child_evt_thd();	/* doesn't return */

	return;
}

//#define UBENCH_ACTIVE 1
#define UBENCH_ITER 10000

static void
sched_ctxt_switch_fn(void *d)
{
	u16_t pid = (u16_t)(u32_t)d;
	while (1) {
		cos_switch_thread(pid, 0);
	}
}

static void
sched_ctxt_switch_ubench(void)
{
	u16_t pid, cid;
	int i;
	u64_t start, end;

	pid = cos_get_thd_id();
	cid = cos_create_thread((int)sched_ctxt_switch_fn, (int)pid, 0);

	rdtscll(start);
	for (i = 0 ; i < UBENCH_ITER ; i++) {
		cos_switch_thread(cid, 0);
	}
	rdtscll(end);
	printc("kernel context switch ubenchmark results: %lld\n", (end-start)/(u64_t)(2*UBENCH_ITER));
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
}

/* Initialize the root scheduler */
int sched_root_init(void)
{
	struct sched_thd *new;

	print_config_info();

	cos_argreg_init();
	sched_init();

	/* switch back to this thread to terminate the system. */
	init = sched_alloc_thd(cos_get_thd_id());
	assert(init);

#ifdef UBENCH_ACTIVE
	sched_ctxt_switch_ubench();
#else	
	sched_init_create_threads(1);
	/* Create the clock tick (timer) thread */
	fp_create_timer();

	new = schedule(NULL);
	cos_switch_thread(new->id, 0);
#endif
	/* Returning will exit the composite system. */
	return 0;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
	{
		sched_timer_tick();
		break;
	}
	case COS_UPCALL_BOOTSTRAP:
	{
		sched_child_init();
		break;
	}
	case COS_UPCALL_CREATE:
		cos_argreg_init();
		((crt_thd_fn_t)arg1)(arg2);
		break;
	case COS_UPCALL_DESTROY:
		fp_kill_thd(sched_get_current());
		break;
	case COS_UPCALL_UNHANDLED_FAULT:
		printc("Unhandled fault occurred, exiting system\n");
		sched_exit();
		break;
	case COS_UPCALL_BRAND_COMPLETE:
		fp_event_completion(sched_get_current());
		break;
	default:
		printc("fp_rr: cos_upcall_fn error - type %x, arg1 %d, arg2 %d\n", 
		      (unsigned int)t, (unsigned int)arg1, (unsigned int)arg2);
		BUG();
		return;
	}

	return;
}
