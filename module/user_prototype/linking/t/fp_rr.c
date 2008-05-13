#include <cos_component.h>
#include <cos_scheduler.h>
//#include <cos_alloc.h>

#define NUM_PRIOS 12
#define LOWEST_PRIO (NUM_PRIOS-1)

#define RUNTIME_SEC 10
#define TIMER_FREQ 100
#define CYC_PER_USEC 2400

#define MAX_REPORTS (8*RUNTIME_SEC)

static volatile unsigned long ticks = 0;
static volatile unsigned long idle_progress = 0;
static volatile unsigned long wakeup_cnt = 0, block_cnt = 0;

static struct sched_thd *timer, *init, *idle, *ds;
static int ds_brand_id;
struct sched_thd blocked;
struct sched_thd upcall_deactive;
struct prio_list {
	struct sched_thd runnable;
} priorities[NUM_PRIOS];


typedef enum {
	MEAS_TYPE_CYCLE,
	MEAS_TYPE_PROGRESS
} meas_type_t;
struct measurement {
	meas_type_t t;
	unsigned int a, b, c;
} ms[MAX_REPORTS];
int curr_meas = 0;

static void record_measurement(meas_type_t type, unsigned int a, unsigned int b, unsigned int c)
{
	if (curr_meas >= MAX_REPORTS) {
		return;
	}

	ms[curr_meas].t = type;
	ms[curr_meas].a = a;
	ms[curr_meas].b = b;
	ms[curr_meas].c = c;
	curr_meas++;
}

static unsigned long same_thd = 0, diff_thd = 0;

static void report_measurement(void)
{
	int i;
	for (i = 0 ; i < MAX_REPORTS ; i++) {
		if (ms[i].a || ms[i].b || ms[i].c) {
			switch (ms[i].t) {
			case MEAS_TYPE_CYCLE:
				print("cycle %d %d %d", ms[i].a, ms[i].b, ms[i].c);
				break;
			case MEAS_TYPE_PROGRESS:
				print("progress %d %d %d", ms[i].a, ms[i].b, ms[i].c);
				break;
			default:
				print("unknown measurement type %d.  %d%d", ms[i].t, 0,0);
			}
		}
	}
	print("Round robin kept at the same thread %d times, and diff %d times. %d",
	      same_thd, diff_thd, 0);
	print("Wakeup %d time, block %d times. %d", wakeup_cnt, block_cnt, 0);
}

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

/*		if (ticks >= 500 && (t->id == 8 || t->id == 10)) {
			print("thread %d updated @ %d with flags %d.", 
			      t->id, ticks, flags);
		}
*/
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
//	print("schedule: highest prio thread is %d. %d%d", t->id, 0,0);
	
	return t;
}

static void fp_print_taskqueue(struct sched_thd *h)
{
	struct sched_thd *iter;

	iter = FIRST_LIST(h, prio_next, prio_prev);
	while (iter != h) {
		record_measurement(MEAS_TYPE_PROGRESS, ticks, iter->id, sched_get_accounting(iter)->progress/*cycles>>10*/);
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

void fp_print_stats(void)
{
	int i;

	for (i = 0 ; i < NUM_PRIOS ; i++) {
		struct sched_thd *head;

		head = &priorities[i].runnable;
		fp_print_taskqueue(head);
	}
	fp_print_taskqueue(&blocked);
	fp_print_taskqueue(&upcall_deactive);
	record_measurement(MEAS_TYPE_PROGRESS, ticks, idle->id, idle_progress);
	record_measurement(MEAS_TYPE_CYCLE, ticks, timer->id, sched_get_accounting(timer)->cycles);
	sched_get_accounting(timer)->cycles = 0;
	idle_progress = 0;
	

	return;
}

//int upcalls_created = 0;

void fp_timer_tick(void)
{
	struct sched_thd *prev, *next;
	int loop;

	/* are we done running? */
	if (ticks >= RUNTIME_SEC*TIMER_FREQ+1) {
		cos_switch_thread(init->id, COS_SCHED_TAILCALL, 0);
	}

	ticks++;

	cos_brand_upcall(ds_brand_id, 0, 0, 0);

	do {
		cos_sched_lock_take();

		if (ticks % 100 == 0) {
/* 			struct sched_thd *t; */
/* 			int lst; */
			
			fp_print_stats();

/* 			if (upcalls_created) { */
/* 				t = fp_find_thread(8, &lst); */
/* 				assert(t); */
/* 				print("interrupt thread %d, flags %d, list %d",  */
/* 				      8, cos_sched_notifications.cos_events[t->event].nfu.v.flags, lst); */
/* 				t = fp_find_thread(10, &lst); */
/* 				assert(t); */
/* 				print("interrupt thread %d, flags %d, list %d",  */
/* 				      10, cos_sched_notifications.cos_events[t->event].nfu.v.flags, lst); */
/* 			} */

			if (ticks == RUNTIME_SEC*TIMER_FREQ) {
				report_measurement();
			}
		}

		prev = sched_get_current();
		cos_sched_process_events(evt_callback, 0);
		next = fp_get_highest_prio();
		/* Chances are good the highest is us */
		if (next == prev) {
			struct sched_thd *t, *r;
			/* the RR part */
			next = fp_get_second_highest_prio(next);
			r = next;
//			print("second is %d %d%d", next->id, 0,0);
			fp_move_end_runnable(next);
			t = fp_get_highest_prio();
			assert(t == prev);
			next = fp_get_second_highest_prio(t);

			assert(sched_get_metric(r)->priority ==
			       sched_get_metric(next)->priority);

			if (r == next) {
				same_thd++;
			} else {
				diff_thd++;
			}
//			print("third is %d %d%d", next->id, 0,0);
		} 
		assert(next != prev);
		

//		print("timer tick switching to %d.   %d%d", next->id,0,0);
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

static void fp_idle_loop(void *d)
{
	//static int last_tick = 0;
	while(1) {
		unsigned long ip;
		do {
			ip = idle_progress;
		} while (cos_cmpxchg(&idle_progress, ip, ip+1) != ip+1);
		
/* 		if (ticks != last_tick) { */
/* 			struct sched_thd *l, *t; */
/* 			int i; */
/* 			struct cos_sched_events *evt; */
/* 			struct cos_se_values *se; */

/* 			cos_sched_lock_take(); */
/* 			last_tick = ticks; */
			
/* 			for (i = 1 ; i < NUM_PRIOS ; i++) { */
/* 				l = &priorities[i].runnable; */
/* 				t = FIRST_LIST(l, prio_next, prio_prev); */
/* 				print("Priority %d. %d%d", i, 0,0); */
/* 				while (t != l) { */
/* 					evt = &cos_sched_notifications.cos_events[t->event]; */
/* 					se = &evt->nfu.v; */
/* 					print("\tthread %d, evt flags %x, %d", t->id, se->flags, 0); */
/* 					t = FIRST_LIST(t, prio_next, prio_prev); */
/* 				} */
/* 			} */

/* 			l = &upcall_deactive; */
/* 			t = FIRST_LIST(l, prio_next, prio_prev); */
/* 			print("Upcall threads %d%d%d", 0, 0, 0); */
/* 			while (t != l) { */
/* 				evt = &cos_sched_notifications.cos_events[t->event]; */
/* 				se = &evt->nfu.v; */
/* 				print("\tthread %d, evt flags %x, %d", t->id, se->flags, 0); */
/* 				t = FIRST_LIST(t, prio_next, prio_prev); */
/* 			} */
/* 			evt = &cos_sched_notifications.cos_events[timer->event]; */
/* 			se = &evt->nfu.v; */
/* 			print("Event for timer has flags %x, next thd %d, urgency %d", */
/* 			      se->flags, se->next, se->urgency); */
/* 			cos_sched_lock_release(); */
/* 		} */
	}
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
static void fp_yield_loop(void *d)
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
	cos_upcall(2);

	return;
}

#define TEST_BRAND
#ifdef TEST_BRAND
static void fp_brand_test_thd(void *d)
{
	static int cnt = 0;
	assert(ds);

//	sched_set_thd_urgency(ds, 3); to test cost of delayed brands
	while (cnt < 10000000) {
		cos_brand_upcall(ds_brand_id, 0, 0, 0);
		cnt++;
	}
	cos_switch_thread(init->id, 0, 0);
}
#endif

static void fp_ds_thd(void *d)
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
	
	wakeup_cnt++;
	//print("thread %d waking up thread %d. %d", cos_get_thd_id(), thd_id, 0);

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

	block_cnt++;
	//print("thread %d blocking. %d%d", cos_get_thd_id(), 0,0);
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

/**** SUPPORT FOR CHILD SCHEDULERS ****/

int sched_create_child_brand(void)
{
	int ucid, bid;
	struct sched_thd *curr, *upcall;
	int prio;

	curr = sched_get_current();
	assert(curr);
	prio = sched_get_metric(curr)->priority;
	bid = cos_brand_cntl(0, COS_BRAND_CREATE, 1);
	ucid = cos_brand_cntl(bid, COS_BRAND_ADD_THD, 0);
	upcall = sched_alloc_upcall_thd(ucid);
	assert(upcall);
	fp_add_evt_thd(upcall, prio);
	sched_alloc_event(upcall);
	sched_add_mapping(ucid, upcall);
	sched_set_thd_urgency(upcall, prio);
	ds_brand_id = bid;
	ds = upcall;

	return ucid;
}

void sched_child_yield_thd(void)
{
	struct sched_thd *curr = sched_get_current();
	assert(curr);
	sched_set_thd_urgency(curr, LOWEST_PRIO);
	fp_change_prio_runnable(curr, LOWEST_PRIO);
	fp_yield_loop(NULL);
}

/*********/

void sched_report_processing(int amnt)
{
	struct sched_thd *t = sched_get_current();

	sched_get_accounting(t)->progress += amnt;
}

//#define LINUX_STYLE

int sched_create_net_upcall(unsigned short int port)
{
	struct sched_thd *t = sched_get_current(), *uc;
	u16_t prio = sched_get_metric(t)->priority;
	unsigned int b_id;

#ifdef LINUX_STYLE
	static struct sched_thd *first = NULL;
#endif
	assert(t);
	uc = sched_setup_upcall_thread(prio-1, prio-1, &b_id, 1);
	assert(uc);
	cos_brand_wire(b_id, COS_HW_NET, port);
	print("Net upcall thread %d with priority %d make for port %d.", 
	      uc->id, sched_get_metric(uc)->priority, port);

#ifdef LINUX_STYLE
	if (!first) {
		first = uc;
	} else {
		if (sched_thd_ready(first)) {
			fp_change_prio_runnable(first, 1);
		} else {
			sched_get_metric(first)->priority = 1;
			sched_set_thd_urgency(first, 1);
		}
		assert(!sched_thd_ready(uc));
		sched_get_metric(uc)->priority = 1;
		sched_set_thd_urgency(uc, 1);
		
//		print("upcalls created!!!! %d%d%d",1,1,1);
//		upcalls_created = 1;
	}
#endif

	//fp_change_prio_runnable(idle, 1);

	return uc->id;
}

int sched_init(void)
{
	static int first = 1;
	int i;
	unsigned int b_id;//, thd_id;
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
	idle = sched_setup_thread(LOWEST_PRIO, LOWEST_PRIO-1, fp_idle_loop);
	print("Idle thread has id %d with priority %d. %d", idle->id, LOWEST_PRIO-1, 0);

#ifndef TEST_BRAND
	new = sched_setup_thread(4, 4, fp_net_thd);
	print("App1 thread has id %d with priority %d. %d", new->id, 4, 0);

	new = sched_setup_thread(6, 6, fp_net_thd);
	print("App2 thread has id %d with priority %d. %d", new->id, 6, 0);

	/* Create the clock tick (timer) thread */
	timer = sched_setup_upcall_thread(0, 0, &b_id, 0);
	print("Timer thread has id %d with priority %d. %d", timer->id, 0, 0);
	cos_brand_wire(b_id, COS_HW_TIMER, 0);
#endif

#ifdef TEST_BRAND
	new = sched_setup_thread(2, 2, fp_brand_test_thd);
	print("App2 thread has id %d with priority %d. %d", new->id, 6, 0);
#endif

	new = sched_setup_thread(1, 1, fp_ds_thd);

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
		print("fp_rr: cos_upcall_fn error - type %x, arg1 %d, arg2 %d", 
		      (unsigned int)t, (unsigned int)arg1, (unsigned int)arg2);
		assert(0);
		return;
	}

	return;
}
