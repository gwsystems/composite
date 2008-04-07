#include <cos_component.h>
#include <cos_scheduler.h>
//#include <cos_alloc.h>

#define NUM_PRIOS 8

static struct sched_thd *current, *timer, *init;
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
		REM_LIST(t, prio_next, prio_prev);

		return t;
	}

	return NULL;
}

static inline struct sched_thd *fp_requeue_get_highest(struct sched_thd *c)
{
	fp_move_end_runnable(c);
	return fp_get_highest_prio();
}

#define RUNTIME_SEC 5
#define TIMER_FREQ 100

static void fp_timer_tick(void)
{
	struct sched_thd *prev;
	static volatile int cnt = 0;

	if (cnt >= RUNTIME_SEC*TIMER_FREQ) {
		cos_switch_thread(init->id, COS_SCHED_TAILCALL, 0);
	}
	cnt++;

	cos_sched_lock_take();

	prev = current;
	current = fp_requeue_get_highest(current);

	if (prev == current) {
		cos_sched_lock_release();
		return;
	} else {
		cos_switch_thread_release(current->id, COS_SCHED_TAILCALL, 0);
	}

	return;
}

typedef void (*crt_thd_fn_t)(void *data);

static void fp_idle_loop(void *d)
{
	while(1) ;
}

static void fp_fresh_thd(void *d)
{
	while (1) {
		struct sched_thd *prev;

		cos_sched_lock_take();
		
		prev = current;
		current = fp_requeue_get_highest(current);

		if (prev != current) {
			cos_switch_thread_release(current->id,0,0);
		} else {
			cos_sched_lock_release();
		}
	}

	return;
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
		sched_init_thd(&priorities[i].runnable, 0);
	}
	sched_init_thd(&blocked, 0);
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



	/* create 3 threads to test rr and fp */
	thd_id = cos_create_thread((int)fp_fresh_thd, 0, 0);
	new = sched_alloc_thd(thd_id);
	fp_add_thd(new, 7);

	thd_id = cos_create_thread((int)fp_fresh_thd, 0, 0);
	new = sched_alloc_thd(thd_id);
	fp_add_thd(new, 7);

	thd_id = cos_create_thread((int)fp_fresh_thd, 0, 0);
	new = sched_alloc_thd(thd_id);
	fp_add_thd(new, 6);

	current = fp_get_highest_prio();
	cos_switch_thread(current->id, 0, 0);

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
