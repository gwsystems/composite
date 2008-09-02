/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <cos_scheduler.h>
//#include <cos_alloc.h>
#include <cos_time.h>

#define NUM_PRIOS 32
#define IDLE_PRIO (NUM_PRIOS-3)
#define CHILD_IDLE_PRIO (NUM_PRIOS-2)
#define GRAVEYARD_PRIO (NUM_PRIOS-1)
#define TIMER_TICK_PRIO (0)
#define TIME_EVENT_PRIO (1)

#define RUNTIME_SEC 60
#define REPORT_FREQ 60
#define TIMER_FREQ 100
#define CYC_PER_USEC 2400

static volatile unsigned long long ticks = 0;
static volatile unsigned long long wakeup_time;
struct sched_thd *wakeup_thd;

static volatile unsigned long wakeup_cnt = 0, block_cnt = 0;

static struct sched_thd *timer, *init, *idle;//, *uc_notif;
struct sched_thd blocked;
struct sched_thd upcall_deactive;
struct prio_list {
	struct sched_thd runnable;
} priorities[NUM_PRIOS];

static void report_data(unsigned int a, unsigned int b, unsigned int c);
static void report_thd_data(unsigned int dataid, unsigned int thdid);
static void report_publish(void);

static inline void fp_add_thd(struct sched_thd *t, unsigned short int prio)
{
	struct sched_thd *tp;

	assert(prio < NUM_PRIOS);
	assert(sched_thd_ready(t));

//	print("add_thd: adding thread %d with priority %d to runlist. %d", 
//	      t->id, prio, 0);
	tp = &(priorities[prio].runnable);
	ADD_LIST(LAST_LIST(tp, prio_next, prio_prev), t, prio_next, prio_prev);
	sched_get_metric(t)->priority = prio;
	
	return;
}

static inline void fp_add_evt_thd(struct sched_thd *t, unsigned short int prio)
{
	assert(prio < NUM_PRIOS);
	assert(!sched_thd_ready(t));
	assert(sched_thd_event(t));

//	print("add_evt_thd: adding thread %d with priority %d to deactive upcall list. %d", 
//	      t->id, prio, 0);
	ADD_LIST(&upcall_deactive, t, prio_next, prio_prev);
	sched_get_metric(t)->priority = prio;

	return;
}

static inline void fp_change_prio_runnable(struct sched_thd *t, unsigned short int prio)
{
	struct sched_metric *sm = sched_get_metric(t);
	struct sched_thd *head;

	assert(prio < NUM_PRIOS);

	sm->priority = prio;
	head = &priorities[prio].runnable;
	REM_LIST(t, prio_next, prio_prev);
	ADD_LIST(LAST_LIST(head, prio_next, prio_prev), t, prio_next, prio_prev);
	sched_set_thd_urgency(t, prio);

	return;
}

static inline void fp_move_end_runnable(struct sched_thd *t)
{
	assert(sched_thd_ready(t));

	fp_change_prio_runnable(t, sched_get_metric(t)->priority);
}

static inline void fp_block_thd(struct sched_thd *t)
{
	assert(sched_thd_ready(t));
	assert(t->wake_cnt == 0);

	t->flags &= ~THD_READY;
	t->flags |= THD_BLOCKED;
	REM_LIST(t, prio_next, prio_prev);
	ADD_LIST(&blocked, t, prio_next, prio_prev);
}

static inline void fp_resume_thd(struct sched_thd *t)
{
	assert(sched_thd_blocked(t));

	t->flags &= ~THD_BLOCKED;
	t->flags |= THD_READY;
	REM_LIST(t, prio_next, prio_prev);
	fp_move_end_runnable(t);
}

static void fp_activate_upcall(struct sched_thd *uc)
{
	if (sched_thd_inactive_evt(uc)) {
		uc->flags &= ~THD_UC_READY;
		uc->flags |= (THD_UC_ACTIVE|THD_READY);
		//REM_LIST(uc, prio_next, prio_prev); //done in move_end_runnable
		fp_move_end_runnable(uc);
	}
}

static void fp_deactivate_upcall(struct sched_thd *uc)
{
	uc->flags &= ~(THD_UC_ACTIVE|THD_READY);
	uc->flags |= THD_UC_READY;
	REM_LIST(uc, prio_next, prio_prev);
	ADD_LIST(&upcall_deactive, uc, prio_next, prio_prev);
}

static inline struct sched_thd *fp_find_non_suspended_list(struct sched_thd *head)
{
	struct sched_thd *t;

	t = FIRST_LIST(head, prio_next, prio_prev);
	while (t != head) {
		if (!sched_thd_suspended(t)) {
			break;
		}
		t = FIRST_LIST(t, prio_next, prio_prev);
	}
	if (t == head) {
		return NULL;
	}

	return t;
}

static struct sched_thd *fp_get_highest_prio(void)
{
	int i;

	for (i = 0 ; i < NUM_PRIOS ; i++) {
		struct sched_thd *t, *dep, *head;

		head = &(priorities[i].runnable);
		if (EMPTY_LIST(head, prio_next, prio_prev)) {
			continue;
		}
		t = fp_find_non_suspended_list(head);
		if (!t) continue;
		
		assert(sched_thd_ready(t));
		assert(sched_get_metric(t));
		assert(sched_get_metric(t)->priority == i);

		/* If t is dependent on another thread's help, run the
		 * depended on thread */
		if ((dep = sched_thd_dependency(t))) {
			return dep;
		}

		return t;
	}

	return NULL;
}

static struct sched_thd *fp_get_second_highest_prio(struct sched_thd *highest)
{
	int i;
	struct sched_thd *tmp, *dep, *head;
	unsigned short int prio;

	assert(fp_get_highest_prio() == highest);
	assert(highest != init);

	/* If the next element isn't the list head, or t, return it */
	prio = sched_get_metric(highest)->priority;
	assert(prio < NUM_PRIOS);
	head = &(priorities[prio].runnable);
	tmp = fp_find_non_suspended_list(highest);//FIRST_LIST(highest, prio_next, prio_prev);
	assert(tmp != highest);
	/* Another thread at same priority */
	if (head != tmp) {
		if ((dep = sched_thd_dependency(tmp))) {
			if (!sched_thd_blocked(dep)) {
				return dep;
			}

// make it so that sched_thd_dependency will detect normal dependencies too between threads, not just for spds....
		} else {
			return tmp;
		}
	}
	/* assumes that idle should always exist */
	assert(prio != NUM_PRIOS-1);

	for (i = prio+1 ; i < NUM_PRIOS ; i++) {
		struct sched_thd *t, *head;

		head = &(priorities[i].runnable);
		if (EMPTY_LIST(head, prio_next, prio_prev)) {
			continue;
		}
		t = fp_find_non_suspended_list(head);
		if (!t) continue;

		assert(sched_thd_ready(t));
		assert(sched_get_metric(t)->priority == i);

		if ((dep = sched_thd_dependency(t))) {
			assert(!sched_thd_blocked(dep));
			return dep;
		}
		return t;
	}

	return NULL;
}

static inline void fp_requeue_highest(void)
{
	fp_move_end_runnable(fp_get_highest_prio());
}

/* scheduler lock should already be taken */
static void evt_callback(struct sched_thd *t, u8_t flags, u32_t cpu_usage)
{
	struct sched_accounting *sa;

	if (flags & (COS_SCHED_EVT_BRAND_ACTIVE|COS_SCHED_EVT_BRAND_READY|COS_SCHED_EVT_BRAND_PEND)) {
		assert(sched_thd_event(t));

		if (flags & COS_SCHED_EVT_BRAND_ACTIVE) {
			fp_activate_upcall(t);
		} else if (flags & COS_SCHED_EVT_BRAND_READY) {
			fp_deactivate_upcall(t);
		} else if (flags & COS_SCHED_EVT_BRAND_PEND) {
			if (t->flags & THD_UC_READY) {
				/* 
				 * The bug is this: upcall is made,
				 * but not immediately executed.  When
				 * it is run (via explicit scheduler
				 * invoction), it will complete.
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
				//print("thread %d marked as ready, but received pending event.%d%d", 
				//    t->id, 0,0);
			}
			fp_activate_upcall(t);
		}
	}

	sa = sched_get_accounting(t);
	sa->cycles += cpu_usage;

	/* if quota has expired, block?? */

	return;
}

/* sched lock should already be taken */
static inline struct sched_thd *fp_schedule(void)
{
	struct sched_thd *t;

	cos_sched_process_events(evt_callback, 0);
	t = fp_get_highest_prio();
	
	return t;
}

#ifdef NIL
static void fp_print_taskqueue(struct sched_thd *h)
{
	struct sched_thd *iter;

	iter = FIRST_LIST(h, prio_next, prio_prev);
	while (iter != h) {
		//record_measurement(MEAS_TYPE_PROGRESS, ticks, iter->id, sched_get_accounting(iter)->progress/*cycles>>10*/);
		sched_get_accounting(iter)->progress = 0;
		iter = FIRST_LIST(iter, prio_next, prio_prev);
	}
}

static struct sched_thd *fp_find_thread_queue(int id, struct sched_thd *h)
{
	struct sched_thd *iter;

	iter = FIRST_LIST(h, prio_next, prio_prev);
	while (iter != h) {
		if (iter->id == id) return iter;
		iter = FIRST_LIST(iter, prio_next, prio_prev);
	}
	return NULL;
}

static struct sched_thd *fp_find_thread(int id, int *list)
{
	struct sched_thd *t;
	int i;

	for (i = 0 ; i < NUM_PRIOS ; i++) {
		t = fp_find_thread_queue(id, &priorities[i].runnable);
		if (t) {
			*list = 0;
			return t;
		}
	}
	t = fp_find_thread_queue(id, &blocked);
	if (t) {
		*list = 1;
		return t;
	}
	t = fp_find_thread_queue(id, &upcall_deactive);
	if (t) {
		*list = 2;
		return t;
	}
	*list = 3;
	return NULL;
}
#endif

static void fp_pre_wakeup(struct sched_thd *t);
static void fp_wakeup(struct sched_thd *thd, spdid_t spdid);

void fp_timer_tick(void)
{
	struct sched_thd *prev, *next;
	int loop;
	int first = 1;

	/* are we done running? */
	if (ticks >= RUNTIME_SEC*TIMER_FREQ+1) {
		cos_switch_thread(init->id, COS_SCHED_TAILCALL, 0);
	}

	do {
		cos_sched_lock_take();

		if (first) {
			first = 0;
			ticks++;

			if (ticks == wakeup_time && wakeup_thd) {
////				print("wakeup time @ %d, current %d. %d", (unsigned int)wakeup_time, (unsigned int)ticks,0);
				wakeup_time = 0;
				fp_pre_wakeup(wakeup_thd);
				if (sched_thd_blocked(wakeup_thd)) {
					fp_wakeup(wakeup_thd, 0);
				}
			}

			if (((unsigned long)ticks) % (REPORT_FREQ*TIMER_FREQ) == (REPORT_FREQ*TIMER_FREQ)-1) {
				report_publish();
			}
		}

		prev = sched_get_current();
		assert(prev);
		cos_sched_process_events(evt_callback, 0);
		next = fp_get_highest_prio();
		assert(next);

		/* Chances are good the highest is us */
		if (next == prev) {
			struct sched_thd *t, *r;
			/* the RR part */
			next = fp_get_second_highest_prio(next);
			r = next;

			fp_move_end_runnable(next);
			t = fp_get_highest_prio();
			assert(t == prev);
			next = fp_get_second_highest_prio(t);

			assert(sched_get_metric(r)->priority ==
			       sched_get_metric(next)->priority);
		} 
		assert(next != prev);

		loop = cos_switch_thread_release(next->id, COS_SCHED_TAILCALL, 0);
		if (loop == -1) {
			print("WTF, timer switch error  %d%d%d",0,0,0);
		}
	} while (loop);

	return;
}

static void fp_event_completion(struct sched_thd *e)
{
	struct sched_thd *next;

	//print("WTF: this should not be happening %d%d%d",0,0,0);
	do {
		cos_sched_lock_take();
		next = fp_schedule();
		if (next == sched_get_current()) {
			next = fp_get_second_highest_prio(next);
//			print("event completion: next,next is %d, current is %d. %d", next->id, cos_get_thd_id(), 0);
		}
	} while (cos_switch_thread_release(next->id, COS_SCHED_TAILCALL, 0));

	return;
}

/* type of newly created thread functions */
typedef void (*crt_thd_fn_t)(void *data);

static void fp_create_spd_thd(void *d)
{
	int spdid = (int)d;

	assert(!cos_upcall(spdid));
}

static void fp_idle_loop(void *d)
{
	while(1);
}

static void fp_yield(void)
{
	struct sched_thd *prev, *next = NULL;
	
	cos_sched_lock_take();

	/* assumes brand thds don't call fp_yield */
	prev = sched_get_current();
	//assert(prev && prev == fp_get_highest_prio());
	if (fp_schedule() == prev) {
		fp_requeue_highest();
	}
	next = fp_get_highest_prio();
	//print("current is %d, orig_thd was %d, next is %d", prev->id, orig_next->id, next->id);
	if (prev != next && next != init) {
		cos_switch_thread_release(next->id, 0, 0);
	} else {
		cos_sched_lock_release();
	}

	return;
}

static void fp_yield_loop(void *d)
{
	while (1) {
		fp_yield();
	}
}

void sched_yield_exec(void)
{
	fp_yield();
}

unsigned int sched_tick_freq(void)
{
	return TIMER_FREQ;
}

static void fp_pre_block(struct sched_thd *thd);
static void fp_block(struct sched_thd *thd, spdid_t spdid);

void sched_timeout(spdid_t spdid, unsigned long amnt)
{
	unsigned long long abs_timeout;
	struct sched_thd *thd, *next;
	int first = 1;

	cos_sched_lock_take();
	
//	print("in sched_timeout, amnt %d, spdid %d.  %d", (unsigned int)amnt, (unsigned int)spdid,0);
	if (0 == amnt) {
		cos_sched_lock_release();
		return;
	}

	thd = sched_get_mapping(cos_get_thd_id());
	assert(thd);

	abs_timeout = ticks + amnt;

////	print("timeout @ %d (currently time %d w/timeout %d).", 
////	      (unsigned int)abs_timeout, (unsigned int)ticks, (unsigned int)wakeup_time);

	if (0 == wakeup_time || abs_timeout < wakeup_time) {
		wakeup_time = abs_timeout;
	}
	/* If we're the timer thread, lets block, otherwise return */
	if (thd != wakeup_thd) {
		cos_sched_lock_release();
		return;
	}

	do {
		/* First time we still hold the lock */
		if (first) {
			first = 0;
		} else {
			cos_sched_lock_take();
		}
		fp_pre_block(thd);
		/* If we've already been woken up, so be it! */
		if (thd->wake_cnt) {
			assert(thd->wake_cnt == 1);
			cos_sched_lock_release();
			return;
		}
		fp_block(thd, cos_spd_id());
//		prints("Timeout loop: pre-schedule");
		next = fp_schedule();
		/* FIXME: This is incorrect if we release the
		 * lock and then are unblocked.  This is a
		 * very uncommon case, and I just want to make
		 * sure it does what is usually expected, thus
		 * this assert */
		assert(next != thd);
//		prints("Timeout loop: switching threads");
	} while (cos_switch_thread_release(next->id, 0, 0));
	
//	prints("timeout thread starting up again.");
	
	return;
}

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
	fp_change_prio_runnable(thd, TIME_EVENT_PRIO);

	cos_sched_lock_release();
	return 0;
}

static void fp_pre_wakeup(struct sched_thd *t)
{
	assert(t->wake_cnt >= 0 && t->wake_cnt <= 2);
	t->wake_cnt++;
	assert(sched_thd_blocked(t) || t->wake_cnt == 2);
}

static void fp_wakeup(struct sched_thd *thd, spdid_t spdid)
{
	assert(thd->wake_cnt == 1);
	/* resume thread, thus no blocking component */
	thd->blocking_component = 0;

////	print("moving thread %d to priority list %d (spdid %d).", 
////	      thd->id, sched_get_metric(thd)->priority, spdid);

	fp_resume_thd(thd);
	report_data(1,0,0);
	report_thd_data(0, thd->id);
}

/* 
 * FIXME: should verify that the blocks and wakes come from the same
 * component
 */
int sched_wakeup(spdid_t spdid, unsigned short int thd_id)
{
	struct sched_thd *thd, *prev, *next;
	int cnt_done = 0;
	
	wakeup_cnt++;

	do {
		cos_sched_lock_take();
		
		//print("thread %d waking up thread %d. %d", cos_get_thd_id(), thd_id, 0);
	
		thd = sched_get_mapping(thd_id);
		if (!thd) goto error;
		
		/* only increase the count once */
		if (!cnt_done) {
			fp_pre_wakeup(thd);
			cnt_done = 1;
		}

//		if (!(thd->blocking_component == 0 || thd->blocking_component == spdid)) {
//			print("blocking component %d, calling spd %d. %d", thd->blocking_component,spdid,0);
//		}

		assert(thd->blocking_component == 0 || 
		       thd->blocking_component == spdid);

		/* If the thd isn't blocked yet (as it was probably
		 * preempted before it could complete the call to
		 * block), no reason to wake it via scheduling
		 */
		if (!sched_thd_blocked(thd)) {
			goto cleanup;
		}

		fp_wakeup(thd, cos_spd_id());
		prev = sched_get_current();
		assert(prev);
		next = fp_schedule();
		if (prev == next) {	
			goto cleanup;
		} 
		/* 
		 * FIXME: if we wake up a thread belonging to another
		 * scheduler, don't schedule, instead make an upcall
		 * into that scheduler 
		 */
	} while (cos_switch_thread_release(next->id, 0, 0));
done:
	return 0;
cleanup:
	cos_sched_lock_release();
	goto done;
error:
	cos_sched_lock_release();
	return -1;
}

static void fp_pre_block(struct sched_thd *thd)
{
	/* A blocked thread can't block... */
	assert(thd->wake_cnt > 0);
	assert(thd->wake_cnt <= 2);
	thd->wake_cnt--;
	thd->block_time = ticks;
}

static void fp_block(struct sched_thd *thd, spdid_t spdid)
{
	assert(thd->wake_cnt == 0);
	thd->blocking_component = spdid;

	fp_block_thd(thd);
	report_data(0,1,0);
	report_thd_data(1, thd->id);
}

/* 
 * FIXME: should verify that the blocks and wakes come from the same
 * component
 */
int sched_block(spdid_t spdid)
{
	struct sched_thd *thd, *next;
	int cnt_done = 0, ret;

	block_cnt++;
	/* 
	 * This needs to be a loop as it's possible that there will be
	 * lock contention, and this thread will schedule itself while
	 * another incriments the wake_cnt 
	 */
	do {
		cos_sched_lock_take();
		
		//print("thread %d blocking. %d%d", cos_get_thd_id(), 0,0);

		thd = sched_get_current();
		if (!thd) goto error;
		
		/* why are we running if blocked */
		assert(sched_thd_ready(thd));

		/* only decrease once, even if we loop */
		if (!cnt_done) {
			fp_pre_block(thd);
			cnt_done = 1;
		}

		assert(thd->blocking_component == 0 || 
		       thd->blocking_component == spdid);

		/* if we already got a wakeup call for this thread */
		if (thd->wake_cnt) {
			assert(thd->wake_cnt == 1);
			cos_sched_lock_release();
			break;
		}
		fp_block(thd, spdid);
		next = fp_schedule();
		assert(next != thd);
	} while (cos_switch_thread_release(next->id, 0, 0));
	/* The amount of time we've blocked */
	ret = ticks - thd->block_time - 1;
	return ret > 0 ? ret : 0;
error:
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

	/* Continue until the critical section is available */
	while (1) {
		cos_sched_lock_take();
		curr = sched_get_current();
		assert(curr);

		/* If the current thread is dependent on another thread, switch to it for help! */
		if (!(holder = sched_take_crit_sect(spdid, curr))) {
			break;
		}
		/* FIXME: proper handling of recursive locking */
		assert(curr != holder);
		report_data(0,0,1);
		curr = holder;
		cos_switch_thread_release(curr->id, 0, 0);
	}
	cos_sched_lock_release();
	return 0;
}

int sched_component_release(spdid_t spdid)
{
	struct sched_thd *curr, *next;

	cos_sched_lock_take();
	curr = sched_get_current();
	assert(curr);

	assert(!sched_release_crit_sect(spdid, curr));
	/* If we woke thread that was waiting for the critical section, switch to it */
	next = fp_schedule();
	if (next != curr) {
		cos_switch_thread_release(next->id, 0, 0);
	} else {
		cos_sched_lock_release();
	}
	return 0;
}

void sched_create(void) {
	return;
}

static struct sched_thd *sched_setup_thread_arg(u16_t priority, u16_t urgency, crt_thd_fn_t fn, void *d)
{
	unsigned int thd_id;
	struct sched_thd *new;

	thd_id = cos_create_thread((int)fn, (int)d, 0);
	new = sched_alloc_thd(thd_id);
	assert(new);
	fp_add_thd(new, priority);
	sched_alloc_event(new);
	sched_add_mapping(thd_id, new);
	sched_set_thd_urgency(new, urgency);

	return new;
}

static struct sched_thd *sched_setup_thread(u16_t priority, u16_t urgency, crt_thd_fn_t fn)
{
	return sched_setup_thread_arg(priority, urgency, fn, 0);
}

static struct sched_thd *sched_setup_upcall_thread(u16_t priority, u16_t urgency, 
						   unsigned int *brand_id, int depth)
{
	unsigned int b_id, thd_id;
	struct sched_thd *upcall;

	b_id = cos_brand_cntl(0, COS_BRAND_CREATE_HW, depth);
	*brand_id = b_id;
	thd_id = cos_brand_cntl(b_id, COS_BRAND_ADD_THD, 0);
	upcall = sched_alloc_upcall_thd(thd_id);
	assert(upcall);
	fp_add_evt_thd(upcall, priority);
	sched_alloc_event(upcall);
	sched_add_mapping(thd_id, upcall);
	sched_set_thd_urgency(upcall, urgency);

	return upcall;
}

/**** SUPPORT FOR CHILD SCHEDULERS ****/

int sched_create_child_brand(int depth)
{
	int ucid, bid;
	struct sched_thd *curr, *upcall;
	int prio;

	curr = sched_get_current();
	assert(curr);
	prio = sched_get_metric(curr)->priority-1;

	bid = cos_brand_cntl(0, COS_BRAND_CREATE, depth);
	ucid = cos_brand_cntl(bid, COS_BRAND_ADD_THD, 0);
	upcall = sched_alloc_upcall_thd(ucid);
	assert(upcall);
	fp_add_evt_thd(upcall, prio);
	sched_alloc_event(upcall);
	sched_add_mapping(ucid, upcall);
	sched_set_thd_urgency(upcall, prio);
//	ds_brand_id = bid;
//	ds = upcall;

	return ucid;
}

void sched_child_yield_thd(void)
{
	struct sched_thd *curr = sched_get_current();
	assert(curr);
	sched_set_thd_urgency(curr, CHILD_IDLE_PRIO);
	fp_change_prio_runnable(curr, CHILD_IDLE_PRIO);
	fp_yield_loop(NULL);
}

void sched_suspend_thd(int thd_id)
{
	struct sched_thd *t;
	
	t = sched_get_mapping(thd_id);
	assert(t);

	cos_sched_lock_take();
	t->flags |= THD_SUSPENDED;
	sched_set_thd_urgency(t, 100/*COS_SCHED_EVT_DISABLED_VAL*/);
	cos_sched_lock_release();

	return;
}

void sched_resume_thd(int thd_id)
{
	struct sched_thd *t;
	
	t = sched_get_mapping(thd_id);
	assert(t);

	cos_sched_lock_take();
	t->flags &= ~THD_SUSPENDED;
	sched_set_thd_urgency(t, sched_get_metric(t)->priority);
	cos_sched_lock_release();

	return;
}

/*********/

void sched_report_processing(int amnt)
{
	struct sched_thd *t = sched_get_current();

	sched_get_accounting(t)->progress += amnt;
}

int sched_create_net_upcall(unsigned short int port, int depth)
{
	struct sched_thd *t = sched_get_current(), *uc;
	u16_t prio = sched_get_metric(t)->priority;
	unsigned int b_id;

	assert(t);
	uc = sched_setup_upcall_thread(prio-1, prio-1, &b_id, depth);
	assert(uc);
	cos_brand_wire(b_id, COS_HW_NET, port);

	print("Net upcall thread %d with priority %d make for port %d.", 
	      uc->id, sched_get_metric(uc)->priority, port);

	cos_sched_cntl(COS_SCHED_GRANT_SCHED, uc->id, 3);

	return uc->id;
}

void sched_exit(void)
{
	cos_switch_thread(init->id, 0, 0);
}

/* 
 * TODO: retrieve the threads from the graveyard when needed, and if
 * not, then make the idle thread reap these threads by killing them
 * (for which a syscall will need to be added to inv.c).
 */
static void fp_kill_thd(void)
{
	struct sched_thd *curr = sched_get_current();
	assert(curr);
	sched_set_thd_urgency(curr, GRAVEYARD_PRIO);
	cos_sched_lock_take();
	fp_change_prio_runnable(curr, GRAVEYARD_PRIO);
	cos_sched_lock_release();
	print("fp_kill_thd: killing %d. %d%d", curr->id, 0,0);
	fp_yield();
}

int sched_init(void)
{
	static int first = 1;
	int i;
	unsigned int b_id;
	struct sched_thd *new, *new2;

	if (!first) return -1;
	first = 0;

	for (i = 0 ; i < NUM_PRIOS ; i++) {
		sched_init_thd(&priorities[i].runnable, 0, 0);
	}
	sched_init_thd(&blocked, 0, 0);
	sched_init_thd(&upcall_deactive, 0, 0);
	sched_ds_init();

	/* switch back to this thread to terminate the system. */
	init = sched_alloc_thd(cos_get_thd_id());

	/* create the idle thread */
	idle = sched_setup_thread(IDLE_PRIO, IDLE_PRIO, fp_idle_loop);
	print("Idle thread has id %d with priority %d. %d", idle->id, IDLE_PRIO, 0);

	/* Create the clock tick (timer) thread */
	timer = sched_setup_upcall_thread(TIMER_TICK_PRIO, TIMER_TICK_PRIO, &b_id, 0);
	print("Timer thread has id %d with priority %d. %d", timer->id, TIMER_TICK_PRIO, TIMER_TICK_PRIO);
	cos_brand_wire(b_id, COS_HW_TIMER, 0);

	/* normal threads: */
	new = sched_setup_thread_arg(TIME_EVENT_PRIO, TIME_EVENT_PRIO, fp_create_spd_thd, (void*)7);
	print("Timeout thread has id %d and priority %d. %d", new->id, 2, 0);

#define N_THDS 16
	new = sched_setup_thread_arg(4, 4, fp_create_spd_thd, (void*)2);
	for (i = 0; i < N_THDS; i++) {
		new2 = sched_setup_thread_arg(4, 4, fp_create_spd_thd, (void*)2);
	}
	print("Worker threads have ids %d to %d @ priority %d.", new->id, new2->id, 4);

	new = fp_get_highest_prio();
	cos_switch_thread(new->id, 0, 0);

	new = sched_setup_thread_arg(TIME_EVENT_PRIO+1, TIME_EVENT_PRIO+1, fp_create_spd_thd, (void*)3);
	cos_switch_thread(new->id, 0,0);
	//print("mpd thread has id %d. %d%d", new->id, 0,0);

	return 0;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
	{
		fp_timer_tick();
		break;
	}
	case COS_UPCALL_BOOTSTRAP:
		sched_init();
		break;
	case COS_UPCALL_CREATE:
		cos_argreg_init();
		((crt_thd_fn_t)arg1)(arg2);
		break;
	case COS_UPCALL_DESTROY:
		fp_kill_thd();
		break;
	case COS_UPCALL_BRAND_COMPLETE:
		fp_event_completion(sched_get_current());
		break;
	default:
		print("fp_rr: cos_upcall_fn error - type %x, arg1 %d, arg2 %d", 
		      (unsigned int)t, (unsigned int)arg1, (unsigned int)arg2);
		assert(0);
		return;
	}

	return;
}

static struct data {
	unsigned int a, b, c;
} d;

static struct thd_data {
	unsigned int data[2];
} t[MAX_NUM_THREADS];

static void report_data(unsigned int a, unsigned int b, unsigned int c)
{
	d.a += a;
	d.b += b;
	d.c += c;
}

static void report_thd_data(unsigned int data_item, unsigned int thdid)
{
	assert(thdid < MAX_NUM_THREADS && data_item < 2);
	t[thdid].data[data_item]++;
}

static void report_publish(void)
{
	int i;
	print("Sched data: %d, %d, %d.", d.a, d.b, d.c);
	d.a = d.b = d.c = 0;

	for (i = 0 ; i < MAX_NUM_THREADS ; i++) {
		if (t[i].data[0] || t[i].data[1]) {
			print("Thread %d: (%d, %d)", i, t[i].data[0], t[i].data[1]);
		}
		t[i].data[0] = t[i].data[1] = 0;
	}
}
