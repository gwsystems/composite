#include <cos_component.h>
#include <cos_scheduler.h>
//#include <cos_alloc.h>

#define NUM_PRIOS 8

static volatile unsigned long ticks = 0;

static struct sched_thd *timer, *init;
struct sched_thd blocked;
struct prio_list {
	struct sched_thd runnable;
} priorities[NUM_PRIOS];

static inline void fp_add_thd(struct sched_thd *t, unsigned short int prio)
{
	struct sched_thd *tp;

	assert(prio < NUM_PRIOS);

	tp = &(priorities[prio].runnable);
	ADD_LIST(LAST_LIST(tp, prio_next, prio_prev), t, prio_next, prio_prev);
	sched_get_metric(t)->priority = prio;
	
	return;
}

static inline void fp_change_prio(struct sched_thd *t, unsigned short int prio)
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
	fp_change_prio(t, sched_get_metric(t)->priority);
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

static struct sched_thd *fp_get_highest_prio(void)
{
	int i;

	for (i = NUM_PRIOS-1 ; i >= 0 ; i--) {
		struct sched_thd *t, *head;

		head = &(priorities[i].runnable);
		if (EMPTY_LIST(head, prio_next, prio_prev)) {
			continue;
		}
		
		t = FIRST_LIST(head, prio_next, prio_prev);
		//REM_LIST(t, prio_next, prio_prev);

		return t;
	}

	return NULL;
}

static inline void fp_requeue_highest(void)
{
	fp_move_end_runnable(fp_get_highest_prio());
}

/* sched lock should already be taken */
static inline struct sched_thd *fp_schedule(void)
{
	return fp_get_highest_prio();
}

static void evt_callback(struct sched_thd *t, u8_t flags, u32_t cpu_usage)
{
	
	return;
}

#define RUNTIME_SEC 5
#define TIMER_FREQ 100
#define CYC_PER_USEC 2400

void fp_timer_tick(void)
{
	struct sched_thd *next;

	if (ticks >= RUNTIME_SEC*TIMER_FREQ) {
		cos_switch_thread(init->id, COS_SCHED_TAILCALL, 0);
	}

	ticks++;

	cos_sched_lock_take();
	fp_requeue_highest();
	next = fp_schedule();
	cos_switch_thread_release(next->id, COS_SCHED_TAILCALL, 0);

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
	if (prev != fp_get_highest_prio()) {
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
 * FIXME: broken broken broken
 *
 * continuously increasing wake_cnt...
 */
int sched_wakeup(unsigned short int thd_id)
{
	struct sched_thd *thd, *prev, *next;

	do {
		cos_sched_lock_take();
		
		thd = sched_get_mapping(thd_id);
		if (!thd) goto error;
		
		thd->wake_cnt++;
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

/* FIXME: same as above - BROKEN */
int sched_block()
{
	struct sched_thd *thd, *next;

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
		
		thd->wake_cnt--;
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

int sched_init(void)
{
	static int first = 1;
	int i;
	unsigned int thd_id;
	struct sched_thd *new;

	if (!first) return -1;
	first = 0;

	for (i = 0 ; i < NUM_PRIOS ; i++) {
		sched_init_thd(&priorities[i].runnable, 0, 0);
	}
	sched_init_thd(&blocked, 0, 0);
	sched_ds_init();

	/* switch back to this thread to terminate the system. */
	init = sched_alloc_thd(cos_get_thd_id());

	/* create the idle thread */
	thd_id = cos_create_thread((int)fp_idle_loop, 0, 0);
	new = sched_alloc_thd(thd_id);
	fp_add_thd(new, 0);

	thd_id = cos_brand_cntl(0, COS_BRAND_CREATE_HW);
	cos_brand_cntl(thd_id, COS_BRAND_ADD_THD);
	cos_brand_wire(thd_id, COS_HW_TIMER, 0);
	timer = sched_alloc_thd(thd_id);
	sched_alloc_event(timer);

	/* create 3 threads to test rr and fp */
	thd_id = cos_create_thread((int)fp_fresh_thd, 0, 0);
	new = sched_alloc_thd(thd_id);
	sched_alloc_event(new);
	fp_add_thd(new, 7);
	sched_add_mapping(thd_id, new);

	thd_id = cos_create_thread((int)fp_fresh_thd, 0, 0);
	new = sched_alloc_thd(thd_id);
	sched_alloc_event(new);
	fp_add_thd(new, 7);
	sched_add_mapping(thd_id, new);

	thd_id = cos_create_thread((int)fp_fresh_thd, 0, 0);
	new = sched_alloc_thd(thd_id);
	sched_alloc_event(new);
	fp_add_thd(new, 7);
	sched_add_mapping(thd_id, new);

	new = fp_get_highest_prio();
	cos_switch_thread(new->id, 0, 0);

	return 0;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
		fp_timer_tick();
		break;
	case COS_UPCALL_BOOTSTRAP:
		sched_init();
		break;
	case COS_UPCALL_CREATE:
		((crt_thd_fn_t)arg1)(arg2);
		break;
	case COS_UPCALL_BRAND_COMPLETE:
		assert(0);
		break;
	default:
		assert(0);
		return;
	}

	return;
}
