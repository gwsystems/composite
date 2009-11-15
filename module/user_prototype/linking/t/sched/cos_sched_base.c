/**
 * Copyright 2008 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu
 * 
 * Copyright The George Washington University, Gabriel Parmer,
 * gparmer@gwu.edu, 2009
 */

#define COS_FMT_PRINT

#include <cos_component.h>
#include <cos_scheduler.h>
//#include <cos_alloc.h>
#include <cos_time.h>
#include <cos_spd_name_map.h>
#include <print.h>

#include <cos_sched_tk.h>

//#define SCHED_DEBUG
#ifdef SCHED_DEBUG
#define PRINTD(s, args...) printc(s, args);
#else
#define PRINTD(s, args...) 
#endif

#define RUNTIME_SEC (6)
#define REPORT_FREQ (1)		/* freq of reporting in seconds */
#define TIMER_FREQ 100
#define CYC_PER_USEC 1000

static volatile unsigned long long ticks = 0;
static volatile unsigned long long wakeup_time;
static struct sched_thd *wakeup_thd;

static struct sched_thd *timer, *init, *idle;
static struct sched_thd blocked;
static struct sched_thd upcall_deactive;

struct sched_ops *scheduler_ops;

#define FPRR_REPORT_EVTS

#ifdef FPRR_REPORT_EVTS

typedef enum {
	NULL_EVT = 0,
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
	SCHED_TARGETTED_DEPENDENCY,
	REVT_LAST
} report_evt_t;

static char *revt_names[] = {
	"null event",
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
	"schedule will explicit target (via dependency)",
	""
};
static long long report_evts[REVT_LAST];

static void report_event(report_evt_t evt)
{
	if (unlikely(evt >= REVT_LAST)) return;

	report_evts[evt]++;
}

extern void mman_print_stats(void);
static void report_output(void)
{
	int i;

	prints("All counters:\n");
	for (i = 0 ; i < REVT_LAST ; i++) {
		printc("\t%s: %lld\n", revt_names[i], report_evts[i]);
		report_evts[i] = 0;
	}

	mman_print_stats();
}
#else
#define report_event(e)
#define report_output()
typedef enum { NULL_EVT = 0 } report_evt_t;
#endif

extern void st_trace_thd(unsigned short int tid);
void print_thd_invframes(struct sched_thd *t)
{
//	unsigned short int tid = t->id;

//	st_trace_thd(tid);
}

static void report_thd_accouting(void)
{
	struct sched_thd *t;

	scheduler_ops->runqueue_print();

	printc("\nBlocked threads (thd, prio, cycles):\n");
	for (t = FIRST_LIST(&blocked, prio_next, prio_prev) ; 
	     t != &blocked ;
	     t = FIRST_LIST(t, prio_next, prio_prev)) {
		if (sched_get_accounting(t)->cycles) {
			printc("\t%d, %d, %lld\n", t->id, 
			       sched_get_metric(t)->priority, sched_get_accounting(t)->cycles);
			print_thd_invframes(t);
			sched_get_accounting(t)->cycles = 0;
		}
	}
	printc("\nInactive upcalls (thd, prio, cycles):\n");
	for (t = FIRST_LIST(&upcall_deactive, prio_next, prio_prev) ; 
	     t != &upcall_deactive ;
	     t = FIRST_LIST(t, prio_next, prio_prev)) {
		if (sched_get_accounting(t)->cycles) {
			printc("\t%d, %d, %lld\n", t->id, 
			       sched_get_metric(t)->priority, sched_get_accounting(t)->cycles);
			print_thd_invframes(t);
			sched_get_accounting(t)->cycles = 0;
		}
	}
	printc("\n");
	report_output();
}

static inline void fp_resume_thd(struct sched_thd *t)
{
	assert(sched_thd_blocked(t));
	assert(!sched_thd_free(t));
	assert(!sched_thd_ready(t));

	t->flags &= ~THD_BLOCKED;
	t->flags |= THD_READY;
	REM_LIST(t, prio_next, prio_prev);
	//fp_move_end_runnable(t);
	if (scheduler_ops->thread_wakeup(t)) assert(0);
}

static void fp_activate_upcall(struct sched_thd *uc)
{
	if (sched_thd_inactive_evt(uc)) {
		uc->flags &= ~THD_UC_READY;
		uc->flags |= THD_READY;
		REM_LIST(uc, prio_next, prio_prev); //done in move_end_runnable
		//fp_move_end_runnable(uc);
		if (scheduler_ops->thread_wakeup(uc)) assert(0);
	}
}

static void fp_deactivate_upcall(struct sched_thd *uc)
{
	uc->flags &= ~THD_READY;
	uc->flags |= THD_UC_READY;
	if (scheduler_ops->thread_block(uc)) assert(0);
	//REM_LIST(uc, prio_next, prio_prev);
	assert(EMPTY_LIST(uc, prio_next, prio_prev));
	ADD_LIST(&upcall_deactive, uc, prio_next, prio_prev);
}

/* scheduler lock should already be taken */
static void evt_callback(struct sched_ops *ops, struct sched_thd *t, u8_t flags, u32_t cpu_usage)
{
	struct sched_accounting *sa;

	if (flags & (COS_SCHED_EVT_BRAND_ACTIVE|COS_SCHED_EVT_BRAND_READY|COS_SCHED_EVT_BRAND_PEND)) {
		if (flags & COS_SCHED_EVT_BRAND_ACTIVE) {
			report_event(BRAND_ACTIVE);
			fp_activate_upcall(t);
		} else if (flags & COS_SCHED_EVT_BRAND_READY) {
			assert(sched_get_current() != t);
			report_event(BRAND_READY);
			fp_deactivate_upcall(t);
		} else if (flags & COS_SCHED_EVT_BRAND_PEND) {
			report_event(BRAND_PENDING);
			if (sched_thd_inactive_evt(t)) {
				/* 
				 * The bug is this: upcall is made,
				 * but not immediately executed.  When
				 * it is run (via explicit scheduler
				 * invocation), it will complete.
				 * Beforehand another interrupt
				 * happens, causing a pending
				 * incriment.  Upcall returns, but
				 * does not execute pending.  There is
				 * no notification that the brand
				 * becomes active again, and the
				 * pending flag that's set when the
				 * upcall completes doesn't register.
				 * Another brand occurs, setting the
				 * pending flag in the shared
				 * structure.  Then the upcall is
				 * awakened.  Problem is that it never
				 * should have stopped executing in
				 * the first place.
				 */
				//printc("thread %d marked as ready, but received pending event.%d%d", 
				//    t->id, 0,0);
			}
			fp_activate_upcall(t);
		}
	}
	report_event(BRAND_CYCLE);
	sa = sched_get_accounting(t);
	sa->cycles += cpu_usage;
	if (ops->time_elapsed(t, cpu_usage)) assert(0);
	/* if quota has expired, block?? */

	return;
}

static void evt_callback_print(struct sched_ops *ops, struct sched_thd *t, u8_t flags, u32_t cpu_usage)
{
	PRINTD("evt callback (curr %d): thd %d, flags %x, usage %d\n", sched_get_current()->id, t->id, flags, cpu_usage);
	evt_callback(ops, t, flags, cpu_usage);
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
static int sched_switch_thread_target(struct sched_ops *ops, int flags, report_evt_t evt, struct sched_thd *target)
{
	struct sched_thd *current = sched_get_current();
	int ret;

	do {
		struct sched_thd *next;

		assert(cos_sched_lock_own());
		/* 
		 * This is subtle: an event might happen _after_ we
		 * check the pending flag here.  If so, then when we
		 * invoke switch_thread, the kernel will return
		 * COS_SCHED_RET_AGAIN, and this will be run again.
		 * In this way, we never miss an event for a
		 * scheduling decision.
		 */
		if (cos_sched_pending_event()) {
			cos_sched_clear_events();
			cos_sched_process_events(evt_callback, ops, 0);
			//TODO: retrieve events from parent scheduler
		}
//		assert(sched_thd_ready(current));
		assert(!sched_thd_free(current));
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
				assert(ops->schedule);
				/* we don't want next to be us! We are an
				 * upcall completing execution */
				next = ops->schedule(current);
				assert(next != current);
			} else {
				next = ops->schedule(NULL);
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
		ret = cos_switch_thread_release(next->id, flags);
		assert(ret != COS_SCHED_RET_ERROR);
		/* success, exit the loop! */
		if (likely(COS_SCHED_RET_SUCCESS == ret)) break;

		cos_sched_lock_take();
		if (evt != NULL_EVT) report_event(evt);
		/* keep looping if we were scheduling using old info */
	} while (unlikely(COS_SCHED_RET_SUCCESS != ret));

	return 0;
done:
	cos_sched_lock_release();
	return 0;
}

static inline int sched_switch_thread(struct sched_ops *ops, int flags, report_evt_t evt)
{
	return sched_switch_thread_target(ops, flags, evt, NULL);
}

/* Should not hold scheduler lock when calling. */
static int fp_kill_thd(struct sched_thd *t)
{
	struct sched_thd *c; 

	cos_sched_lock_take();
	c = sched_get_current();
	if (scheduler_ops->thread_remove(t)) assert(0);

	sched_switch_thread(scheduler_ops, 0, NULL_EVT);
	if (t == c) assert(0);

	return 0;
}

static void fp_pre_wakeup(struct sched_thd *t);
static void fp_wakeup(struct sched_thd *thd, spdid_t spdid);

static void sched_timer_tick(void)
{
	while (1) {
		cos_sched_lock_take();
		
		report_event(TIMER_TICK);
		
		if ((ticks % (REPORT_FREQ*TIMER_FREQ)) == ((REPORT_FREQ*TIMER_FREQ)-1)) {
			report_thd_accouting();
			cos_stats();
		}
		
		/* are we done running? */
		if (ticks >= RUNTIME_SEC*TIMER_FREQ+1) {
			while (COS_SCHED_RET_SUCCESS != 
			       cos_switch_thread_release(init->id, COS_SCHED_BRAND_WAIT)) {
				cos_sched_lock_take();
				if (cos_sched_pending_event()) {
					cos_sched_clear_events();
					cos_sched_process_events(evt_callback, scheduler_ops, 0);
				}
			}
		}
		
		ticks++;
		/* Wakeup the event thread? */
		if (ticks == wakeup_time && wakeup_thd) {
			wakeup_time = 0;
			fp_pre_wakeup(wakeup_thd);
			/* If the event thread has not blocked yet, then don't
			 * wake it fully */
			if (sched_thd_blocked(wakeup_thd)) {
				fp_wakeup(wakeup_thd, 0);
			}
		}
		
		sched_switch_thread(scheduler_ops, COS_SCHED_BRAND_WAIT, NULL_EVT);
	}
}

static void fp_event_completion(struct sched_thd *e)
{
	report_event(EVT_CMPLETE);

	cos_sched_lock_take();
	sched_switch_thread(scheduler_ops, COS_SCHED_TAILCALL, EVT_CMPLETE_LOOP);
	assert(0);

	return;
}

/* type of newly created thread functions */
typedef void (*crt_thd_fn_t)(void *data);

static void fp_timer(void *d)
{
	printc("Starting timer\n");
	sched_timer_tick();
	assert(0);
}

static void fp_create_spd_thd(void *d)
{
	int spdid = (int)d;

	if (cos_upcall(spdid)) {
		prints("fprr: error making upcall into spd.\n");
	}
}

static void fp_idle_loop(void *d)
{
	while(1) {
		/* Unfortunately, we can't make this strong an
		 * assertion.  Instead, we really can only assert that
		 * non-upcall-complete events shouldn't happen
		assert(!cos_sched_pending_event());  */
		if (cos_sched_pending_event()) {
			report_event(IDLE_SCHED);
 			cos_sched_lock_take();
			sched_switch_thread(scheduler_ops, 0, IDLE_SCHED_LOOP);
		}
		cos_idle();
	}
}

unsigned int sched_tick_freq(void)
{
	return TIMER_FREQ;
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
	sched_switch_thread(scheduler_ops, 0, TIMEOUT_LOOP);	

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
	t->wake_cnt++;
	if (!sched_thd_dependent(t) &&
	    !(sched_thd_blocked(t) || t->wake_cnt == 2)) {
		printc("thread %d (from thd %d) has wake_cnt %d\n", 
		       t->id, cos_get_thd_id(), t->wake_cnt);
		assert(0);
	}
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
	assert(thd->blocking_component == 0 || thd->blocking_component == spdid);
	
	if (thd->dependency_thd) {
		assert(sched_thd_dependent(thd));
		thd->dependency_thd = NULL;
		thd->flags &= ~THD_DEPENDENCY;
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
		cos_sched_cntl(COS_SCHED_BREAK_PREEMPTION_CHAIN, 0, 0);
		fp_wakeup(thd, spdid);
	}

	sched_switch_thread(scheduler_ops, 0, WAKE_LOOP);
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
	if (scheduler_ops->thread_block(t)) assert(0);
	assert(EMPTY_LIST(t, prio_next, prio_prev));
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
	struct sched_thd *thd, *dep;
	int ret;
	int first = 1;

	cos_sched_lock_take();
	thd = sched_get_current();
	assert(thd);
	
	/* we shouldn't block while holding a component lock */
	if (unlikely(0 != thd->contended_component)) goto err;
	if (unlikely(!(thd->blocking_component == 0 || 
		       thd->blocking_component == spdid))) goto err;
	assert(!sched_thd_free(thd));
	assert(!sched_thd_blocked(thd));
	fp_pre_block(thd);
	/* if we already got a wakeup call for this thread */
	if (thd->wake_cnt) {
		assert(thd->wake_cnt == 1);
		cos_sched_lock_release();
		return 0;
	}
	/* dependencies keep the thread on the runqueue, so
	 * that it can be selected to execute and its
	 * dependency list walked. */
	if (dependency_thd) {
		dep = sched_get_mapping(dependency_thd);
		if (!dep) {
			fp_pre_wakeup(thd);
			goto err;
		}
		thd->dependency_thd = dep;
		thd->flags |= THD_DEPENDENCY;
	} else {
		fp_block(thd, spdid);
	}
		
	while (0 == thd->wake_cnt) {
		if (dependency_thd) {
			assert(dep && dep == thd->dependency_thd);
			sched_switch_thread_target(scheduler_ops, 0, BLOCK_LOOP, dep);
			cos_sched_lock_take();
			report_event(BLOCKED_W_DEPENDENCY);
			if (!first) report_event(BLOCKED_DEP_RETRY);
		} else {
			sched_switch_thread(scheduler_ops, 0, BLOCK_LOOP);
			cos_sched_lock_take();
			if (!first) report_event(RETRY_BLOCK);
		}
		first = 0;
	}
	assert(thd->wake_cnt == 1);
	/* The amount of time we've blocked */
	ret = ticks - thd->block_time - 1;
	cos_sched_lock_release();

	return ret > 0 ? ret : 0;
err:
	cos_sched_lock_release();
	return -1;
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

	cos_sched_lock_take();
	report_event(COMP_TAKE);
	curr = sched_get_current();
	assert(curr);
	assert(!sched_thd_blocked(curr));

	/* Continue until the critical section is available */
	while (1) {
		report_event(COMP_TAKE_ATTEMPT);
		/* If the current thread is dependent on another thread, switch to it for help! */
		holder = sched_take_crit_sect(spdid, curr);
		if (NULL == holder) break;

		/* FIXME: proper handling of recursive locking */
		assert(curr != holder);
		report_event(COMP_TAKE_CONTENTION);
		sched_switch_thread_target(scheduler_ops, 0, NULL_EVT, holder);
		cos_sched_lock_take();
		report_event(COMP_TAKE_LOOP);
	}
	cos_sched_lock_release();
	return 0;
}

int sched_component_release(spdid_t spdid)
{
	struct sched_thd *curr;

	report_event(COMP_RELEASE);
	cos_sched_lock_take();
	curr = sched_get_current();
	assert(curr);

	if (sched_release_crit_sect(spdid, curr)) {
		prints("fprr: error releasing spd's critical section\n");
	}
	sched_switch_thread(scheduler_ops, 0, NULL_EVT);

	return 0;
}

static struct sched_thd *sched_setup_thread_arg(char *metric_str, crt_thd_fn_t fn, void *d)
{
	unsigned int thd_id;
	struct sched_thd *new;

	thd_id = cos_create_thread((int)fn, (int)d, 0);
	new = sched_alloc_thd(thd_id);
	assert(new);
//	fp_add_thd(new, priority);
//	fp_new_thd(new);
	if (0 > sched_alloc_event(new)) assert(0);
	sched_add_mapping(thd_id, new);
	scheduler_ops->thread_params_set(new, metric_str);
//	sched_get_metric(new)->priority = priority;
//	sched_get_metric(new)->urgency = urgency;
	scheduler_ops->thread_new(new);
//	sched_set_thd_urgency(new, urgency);

	return new;
}

static struct sched_thd *sched_setup_thread(char *metric_str, crt_thd_fn_t fn)
{
	return sched_setup_thread_arg(metric_str, fn, 0);
}

/* this should just create the thread, not set the prio...there should
 * be a separate function for that */
int sched_create_thread(spdid_t spdid, struct cos_array *data)
{
	struct sched_thd *curr, *new;
	/* well this is just stupid...thx gcc */
	void *d = (void*)(int)spdid;
	char *metric_str;

	if (!cos_argreg_arr_intern(data)) return -1;
	if (((char *)data->mem)[data->sz-1] != '\0') return -1;

	cos_sched_lock_take();
	curr = sched_get_current();
	metric_str = (char *)data->mem;
	new = sched_setup_thread_arg((char *)metric_str, fp_create_spd_thd, d);
	cos_sched_lock_release();
	printc("fprr: created thread %d in spdid %d (requested by %d)\n",
	       new->id, spdid, curr->id);

	return new->id;
}

/* return the id of the brand created */
static int sched_setup_brand(spdid_t spdid)
{
	unsigned short int b_id;

	b_id = cos_brand_cntl(COS_BRAND_CREATE_HW, 0, 0, spdid);

	return b_id;
}

unsigned long sched_timestamp(void)
{
	return (unsigned long)ticks;
}

int sched_create_net_brand(spdid_t spdid, unsigned short int port)
{
	int b_id;

	b_id = sched_setup_brand(spdid);
	cos_brand_wire(b_id, COS_HW_NET, port);

	return b_id;
}

int sched_add_thd_to_brand(spdid_t spdid, unsigned short int bid, unsigned short int tid)
{
	struct sched_thd *t;
	int ret;

	ret = cos_brand_cntl(COS_BRAND_ADD_THD, bid, tid, 0);
	if (0 > ret) return -1;
	t = sched_get_mapping(tid);
	if (NULL == t) return -1;
	return 0;
}

void sched_exit(void)
{
	cos_switch_thread(init->id, 0);
}

static struct sched_thd *fp_init_component(spdid_t spdid, char *metric_str)
{
	struct sched_thd *new;

	assert(spdid > 0);
	new = sched_setup_thread_arg(metric_str, fp_create_spd_thd, (void*)(int)spdid);
	printc("component %d's thread has id %d and priority %s.\n", spdid, new->id, metric_str);
	
	return new;
}

static struct sched_thd *fp_create_timer(void)
{
	int bid;

	bid = sched_setup_brand(cos_spd_id());
	timer = sched_setup_thread_arg("t", fp_timer, (void*)bid);
	if (NULL == timer) assert(0);
	if (0 > sched_add_thd_to_brand(cos_spd_id(), bid, timer->id)) assert(0);
	printc("Timer thread has id %d with priority %s.\n", timer->id, "t");
	cos_brand_wire(bid, COS_HW_TIMER, 0);

	return timer;
}

int sched_init(void)
{
	static int first = 1;
	int i = 0;
	spdid_t ret;
	struct sched_thd *new;

//#define MICRO_INV
#ifdef MICRO_INV
#define MICRO_ITER 1000000
	extern void print_null(void);
	{
		assert(!cos_mpd_cntl(COS_MPD_MERGE, 1, 6));
		int i;
		for (i = 0 ; i < MICRO_ITER ; i++) {
			print_null();
		}
	}
	return 0;
#endif

	assert(first);
	first = 0;

	sched_init_thd(&blocked, 0, THD_FREE);
	sched_init_thd(&upcall_deactive, 0, THD_FREE);
	sched_ds_init();

	scheduler_ops = sched_initialization();
	assert(scheduler_ops);

	/* switch back to this thread to terminate the system. */
	init = sched_alloc_thd(cos_get_thd_id());
	assert(init);

	/* create the idle thread */
	idle = sched_setup_thread("i", fp_idle_loop);
	printc("Idle thread has id %d with priority %s.\n", idle->id, "i");

	extern spdid_t sched_comp_config(spdid_t spdid, int index, struct cos_array *data);
	#define SCHED_STR_SZ 64
	do {
		struct cos_array *data;

		data = cos_argreg_alloc(sizeof(struct cos_array) + SCHED_STR_SZ);
		assert(data);
		data->sz = SCHED_STR_SZ;
		ret = sched_comp_config(cos_spd_id(), i++, data);
		printc("index %d yielded spd %d\n", i-1, ret);
		if (ret > 0 && data->sz > 0) {
			fp_init_component(ret, data->mem);
		}
		cos_argreg_free(data);
	} while (ret > 0);

	/* Create the clock tick (timer) thread */
	fp_create_timer();

	new = scheduler_ops->schedule(NULL);
	cos_switch_thread(new->id, 0);

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
		cos_argreg_init();
		sched_init();
		break;
	case COS_UPCALL_CREATE:
		cos_argreg_init();
		((crt_thd_fn_t)arg1)(arg2);
		break;
	case COS_UPCALL_DESTROY:
		fp_kill_thd(sched_get_current());
		break;
	case COS_UPCALL_BRAND_COMPLETE:
		fp_event_completion(sched_get_current());
		break;
	default:
		printc("fp_rr: cos_upcall_fn error - type %x, arg1 %d, arg2 %d\n", 
		      (unsigned int)t, (unsigned int)arg1, (unsigned int)arg2);
		assert(0);
		return;
	}

	return;
}
