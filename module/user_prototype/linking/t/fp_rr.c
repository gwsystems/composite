#include <cos_component.h>
#include <cos_scheduler.h>
//#include <cos_alloc.h>

#define NUM_PRIOS 8
#define LOWEST_PRIO (NUM_PRIOS-1)

static volatile unsigned long ticks = 0;

static struct sched_thd *timer, *init;
struct sched_thd blocked;
struct sched_thd upcall_deactive;
struct prio_list {
	struct sched_thd runnable;
} priorities[NUM_PRIOS];

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

static struct sched_thd *fp_get_highest_prio(void)
{
	int i;

	for (i = 0 ; i < NUM_PRIOS ; i++) {
		struct sched_thd *t, *head;

		head = &(priorities[i].runnable);
		if (EMPTY_LIST(head, prio_next, prio_prev)) {
			continue;
		}
		t = FIRST_LIST(head, prio_next, prio_prev);

		assert(sched_thd_ready(t));
		assert(sched_get_metric(t));
		assert(sched_get_metric(t)->priority == i);

		return t;
	}

	return NULL;
}

static struct sched_thd *fp_get_second_highest_prio(struct sched_thd *highest)
{
	int i;
	struct sched_thd *tmp, *head;
	unsigned short int prio;

	assert(fp_get_highest_prio() == highest);
	assert(highest != init);

	/* If the next element isn't the list head, or t, return it */
	prio = sched_get_metric(highest)->priority;
	assert(prio < NUM_PRIOS);
	head = &(priorities[prio].runnable);
	tmp = FIRST_LIST(highest, prio_next, prio_prev);
	assert(tmp != highest);
	/* Another thread at same priority */
	if (head != tmp) {
		return tmp;
	}
	/* assumes that idle should always exist */
	assert(prio != NUM_PRIOS-1);

	for (i = prio+1 ; i < NUM_PRIOS ; i++) {
		struct sched_thd *t, *head;

		head = &(priorities[i].runnable);
		if (EMPTY_LIST(head, prio_next, prio_prev)) {
			continue;
		}
		t = FIRST_LIST(head, prio_next, prio_prev);

		assert(sched_thd_ready(t));
		assert(sched_get_metric(t)->priority == i);

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
/*	static struct sched_thd *evt_thd = NULL;

	if (!evt_thd) evt_thd = t;

	if (t != evt_thd) {
		print("WTF, new thread %d getting event, original is %d.  %d",
		      t->id, evt_thd->id, 0);
	}
*/
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
				print("thread %d marked as ready, but received pending event.%d%d", 
				      t->id, 0,0);
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
//	print("schedule: highest prio thread is %d. %d%d", t->id, 0,0);
	
	return t;
}

#define RUNTIME_SEC 5
#define TIMER_FREQ 100
#define CYC_PER_USEC 2400

void fp_timer_tick(void)
{
	struct sched_thd *prev, *next;
	int loop;

	if (ticks >= RUNTIME_SEC*TIMER_FREQ) {
		cos_switch_thread(init->id, COS_SCHED_TAILCALL, 0);
	}

	ticks++;

	do {
		cos_sched_lock_take();
		prev = sched_get_current();
		cos_sched_process_events(evt_callback, 0);
		next = fp_get_highest_prio();
		/* Chances are good the highest is us */
		if (next == prev) {
			/* the RR part */
			next = fp_get_second_highest_prio(next);
//			print("second is %d %d%d", next->id, 0,0);
			fp_move_end_runnable(next);
			next = fp_get_second_highest_prio(fp_get_highest_prio());
//			print("third is %d %d%d", next->id, 0,0);
		}
		assert(next != prev);

//		print("timer tick switching to %d.   %d%d", next->id,0,0);
		loop = cos_switch_thread_release(next->id, COS_SCHED_TAILCALL, 0);
		if (loop == 1) {
			print("wtf, timer self switch  %d%d%d",0,0,0);
		} else if (loop == -1) {
			print("WTF, timer switch error  %d%d%d",0,0,0);
		}
	} while (loop);

	return;
}

static void fp_event_completion(struct sched_thd *e)
{
	struct sched_thd *next;

	print("WTF: this should not be happening %d%d%d",0,0,0);
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

static void fp_idle_loop(void *d)
{
	while(1) ;
}

static void fp_yield(void)
{
	struct sched_thd *prev, *next = NULL;
	
	cos_sched_lock_take();
#ifdef NIL
	{
		struct sched_thd *l, *t;
		int i;
		struct cos_sched_events *evt;
		struct cos_se_values *se;
		
		for (i = 1 ; i < 2/*NUM_PRIOS*/ ; i++) {
			l = &priorities[i].runnable;
			t = FIRST_LIST(l, prio_next, prio_prev);
			print("Priority %d. %d%d", i, 0,0);
			while (t != l) {
				print("\tthread %d, %d%d", t->id, 0,0);
				t = FIRST_LIST(t, prio_next, prio_prev);
			}
		}

		l = &upcall_deactive;
		t = FIRST_LIST(l, prio_next, prio_prev);
		print("Upcall threads %d%d%d", 0, 0, 0);
		while (t != l) {
			print("\tthread %d, %d%d", t->id, 0,0);
			t = FIRST_LIST(t, prio_next, prio_prev);
		}
		evt = &cos_sched_notifications.cos_events[timer->event];
		se = &evt->nfu.v;
		print("Event for timer has flags %x, next thd %d, urgency %d",
		      se->flags, se->next, se->urgency);

		//cos_switch_thread_release(init->id, 0, 0);
	}
#endif
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

//#define DELAY 1000000
#define DELAY 0
volatile long bar = 0;
static void fp_fresh_thd(void *d)
{
	while (1) {
		int cnt = 0;

//		print("Running! %d%d%d", 0,0,0);
		fp_yield();
//		print("begin running after%d%d%d", 0,0,0);
		while (cnt++ < DELAY) bar++;
		// run thread's action
	}
}

static void fp_net_thd(void *d)
{
	cos_upcall(3);

	return;
}


/* 
 * FIXME: should verify that the blocks and wakes come from the same
 * component
 */
int sched_wakeup(unsigned short int thd_id)
{
	struct sched_thd *thd, *prev, *next;
	int cnt_done = 0;

	print("thread %d waking up thread %d. %d", cos_get_thd_id(), thd_id, 0);

	do {
		cos_sched_lock_take();
		
		thd = sched_get_mapping(thd_id);
		if (!thd) goto error;
		
		/* only do once */
		if (!cnt_done) {
			thd->wake_cnt++;
			cnt_done = 1;
		}
		/* If the thd isn't blocked yet, no reason to wake it via
		 * scheduling
		 */
		if (!sched_thd_blocked(thd)) {
			goto cleanup;
		}
		
		fp_resume_thd(thd);
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

/* 
 * FIXME: should verify that the blocks and wakes come from the same
 * component
 */
int sched_block()
{
	struct sched_thd *thd, *next;
	int cnt_done = 0;

	print("thread %d blocking. %d%d", cos_get_thd_id(), 0,0);
	/* 
	 * This needs to be a loop as it's possible that there will be
	 * lock contention, and this thread will schedule itself while
	 * another incriments the wake_cnt 
	 */
	do {
		cos_sched_lock_take();
		
		thd = sched_get_current();
		if (!thd) goto error;
		
		/* why are we running if blocked */
		assert(sched_thd_ready(thd));

		/* only decrease once */
		if (!cnt_done) {
			assert(thd->wake_cnt >= 0);
			thd->wake_cnt--;
			cnt_done = 1;
		}

		if (thd->wake_cnt) {
			goto cleanup;
		}
		
		fp_block_thd(thd);
		next = fp_schedule();
		assert(next != thd);
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

static struct sched_thd *sched_setup_thread(u16_t priority, u16_t urgency, crt_thd_fn_t fn)
{
	unsigned int thd_id;
	struct sched_thd *new;

	thd_id = cos_create_thread((int)fn, 0, 0);
	new = sched_alloc_thd(thd_id);
	assert(new);
	fp_add_thd(new, priority);
	sched_alloc_event(new);
	sched_add_mapping(thd_id, new);
	sched_set_thd_urgency(new, urgency);

	return new;
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

int sched_create_net_upcall(unsigned short int port)
{
	struct sched_thd *t = sched_get_current(), *uc;
	u16_t prio = sched_get_metric(t)->priority;
	unsigned int b_id;

	assert(t);
	uc = sched_setup_upcall_thread(prio-1, prio-1, &b_id, 1);
	assert(uc);
	cos_brand_wire(b_id, COS_HW_NET, port);
	print("creating brand and net upcall thread %d for port %d.  %d", 
	      uc->id, port, 0);

	return uc->id;
}

int sched_init(void)
{
	static int first = 1;
	int i;
	unsigned int b_id, thd_id;
	struct sched_thd *new;

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
	thd_id = cos_create_thread((int)fp_idle_loop, 0, 0);
	new = sched_alloc_thd(thd_id);
	fp_add_thd(new, LOWEST_PRIO);

	/* create 3 threads to test rr and fp */
//	sched_setup_thread(2, 2);
//	sched_setup_thread(1, 1);
	sched_setup_thread(2, 2, fp_net_thd);
	//sched_setup_thread(4, 4, fp_net_thd);

	/* Create the clock tick (timer) thread */
	timer = sched_setup_upcall_thread(0, 0, &b_id, 0);
	cos_brand_wire(b_id, COS_HW_TIMER, 0);

	new = fp_get_highest_prio();
	cos_switch_thread(new->id, 0, 0);

	return 0;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
	{
		static int first = 1;

		if (sched_get_current() != timer) {
			print("sched_get_current %d, timer %d. %d", (unsigned int)cos_get_thd_id(), (unsigned int)timer->id, 1);
		}
		assert(sched_get_current() == timer);
		if (first) {
			cos_argreg_init();
			first = 0;
		}
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
	case COS_UPCALL_BRAND_COMPLETE:
		fp_event_completion(sched_get_current());
		break;
	default:
		assert(0);
		return;
	}

	return;
}
