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
	uc->flags &= ~THD_UC_READY;
	uc->flags |= (THD_UC_ACTIVE|THD_READY);
	REM_LIST(uc, prio_next, prio_prev);
	fp_move_end_runnable(uc);
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

	/* If the next element isn't the list head, or t, return it */
	prio = sched_get_metric(highest)->priority;
	assert(prio < NUM_PRIOS);
	head = &(priorities[prio].runnable);
	tmp = FIRST_LIST(highest, prio_next, prio_prev);
	assert(tmp != highest);
	if (head != tmp) {
		return tmp;
	}
	if (prio == NUM_PRIOS-1) {
		return NULL;
	}
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

	if (flags & (COS_SCHED_EVT_BRAND_ACTIVE|COS_SCHED_EVT_BRAND_READY)) {
		assert(sched_thd_event(t));

		if (flags & COS_SCHED_EVT_BRAND_ACTIVE) {
			fp_activate_upcall(t);
		} else if (flags & COS_SCHED_EVT_BRAND_READY) {
			fp_deactivate_upcall(t);
		}
	}

	sa = sched_get_accounting(t);
	sa->cycles += cpu_usage;

//	print("evt for %d, w/ flags %d, current is %d", t->id, flags, sched_get_current()->id);
	/* if quota has expired, block?? */

	return;
}

/* sched lock should already be taken */
static inline struct sched_thd *fp_schedule(void)
{
	struct sched_thd *t;

	cos_sched_process_events(evt_callback, 0);
	t = fp_get_highest_prio();
//	print("fp_sched: current is %d, next is %d. %d", cos_get_thd_id(), t->id, 0);

	return t;
}

#define RUNTIME_SEC 100
#define TIMER_FREQ 100
#define CYC_PER_USEC 2400

void fp_timer_tick(void)
{
	struct sched_thd *next;

	if (ticks >= RUNTIME_SEC*TIMER_FREQ) {
		cos_switch_thread(init->id, COS_SCHED_TAILCALL, 0);
	}

	ticks++;

	do {
		cos_sched_lock_take();
		/* the RR part */
		fp_requeue_highest();
		next = fp_schedule();
		if (next == sched_get_current()) {
			next = fp_get_second_highest_prio(next);
			print("timer tick: next,next is %d, current is %d. %d", next->id, cos_get_thd_id(), 0);
		}
	} while (cos_switch_thread_release(next->id, COS_SCHED_TAILCALL, 0));

	return;
}

static void fp_event_completion(struct sched_thd *e)
{
	struct sched_thd *next;

	do {
		cos_sched_lock_take();
		next = fp_schedule();
		if (next == sched_get_current()) {
			next = fp_get_second_highest_prio(next);
			print("event completion: next,next is %d, current is %d. %d", next->id, cos_get_thd_id(), 0);
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
	struct sched_thd *prev, *next;
	
	cos_sched_lock_take();
	
	/* assumes brand thds don't call fp_yield */
	prev = sched_get_current();
	assert(prev);
	next = fp_get_highest_prio();
	if (prev != next) {
		assert(0);
	}

	fp_requeue_highest();
	next = fp_schedule();
	if (prev != next) {
		cos_switch_thread_release(next->id, 0, 0);
	} else {
		cos_sched_lock_release();
	}
}

static void fp_fresh_thd(void *d)
{
	while (1) {
		fp_yield();

		// run thread's action
	}
}

/* 
 * FIXME: should verify that the blocks and wakes come from the same
 * component
 */
int sched_wakeup(unsigned short int thd_id)
{
	struct sched_thd *thd, *prev, *next;
	int cnt_done = 0;

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

static struct sched_thd *sched_setup_thread(u16_t priority, u16_t urgency)
{
	unsigned int thd_id;
	struct sched_thd *new;

	thd_id = cos_create_thread((int)fp_fresh_thd, 0, 0);
	new = sched_alloc_thd(thd_id);
	assert(new);
	fp_add_thd(new, priority);
	sched_alloc_event(new);
	sched_add_mapping(thd_id, new);
	sched_set_thd_urgency(new, urgency);

	return new;
}

static struct sched_thd *sched_setup_upcall_thread(u16_t priority, u16_t urgency, 
						   unsigned int *brand_id)
{
	unsigned int b_id, thd_id;
	struct sched_thd *upcall;

	b_id = cos_brand_cntl(0, COS_BRAND_CREATE_HW);
	*brand_id = b_id;
	thd_id = cos_brand_cntl(b_id, COS_BRAND_ADD_THD);
	upcall = sched_alloc_upcall_thd(thd_id);
	assert(upcall);
	fp_add_evt_thd(upcall, priority);
	sched_alloc_event(upcall);
	sched_add_mapping(thd_id, upcall);
	sched_set_thd_urgency(upcall, urgency);

	return upcall;
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
	sched_setup_thread(0, 0);
	sched_setup_thread(0, 0);
	sched_setup_thread(0, 2);

	/* Create the clock tick (timer) thread */
	timer = sched_setup_upcall_thread(0, 1, &b_id);
	cos_brand_wire(b_id, COS_HW_TIMER, 0);

	new = fp_get_highest_prio();
	cos_switch_thread(new->id, 0, 0);

	return 0;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
		if (sched_get_current() != timer) {
			print("sched_get_current %d, timer %d. %d", (unsigned int)cos_get_thd_id(), (unsigned int)timer->id, 1);
		}
		assert(sched_get_current() == timer);
		fp_timer_tick();
		break;
	case COS_UPCALL_BOOTSTRAP:
		sched_init();
		break;
	case COS_UPCALL_CREATE:
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
