#include <cos_types.h>
#include <cos_component.h>
#include <part_task.h>
#include <part.h>
#include <../interface/capmgr/memmgr.h>

#include <sl.h>
#include <sl_xcore.h>

#define PART_MAX_PAGES (((PART_MAX_TASKS * sizeof(struct part_task)) / PAGE_SIZE) + 1)
#define PART_MAX_DATA_PAGES (((PART_MAX_TASKS * sizeof(struct part_data)) / PAGE_SIZE) + 1)

struct deque_part part_dq_percore[NUM_CPU];
//struct cirque_par parcq_global;
static volatile unsigned part_ready = 0;
volatile int in_main_parallel;
#if defined(PART_ENABLE_NESTED)
struct crt_lock part_l_lock;
struct ps_list_head part_l_global;
#else
struct part_task main_task;
#endif
static struct part_task *part_tasks = NULL;
static struct part_data *part__data = NULL;
struct ps_list_head part_thdpool_core[NUM_CPU];

#define PART_DEQUE_SZ 64
#define _PART_PRIO TCAP_PRIO_MAX
#define _PART_PRIO_PACK() sched_param_pack(SCHEDP_PRIO, _PART_PRIO)

#define _PART_IDLE_PRIO (_PART_PRIO+4)
#define _PART_IDLE_PRIO_PACK() sched_param_pack(SCHEDP_PRIO, _PART_IDLE_PRIO)

/* idle thread to wakeup when there is nothing to do on this core! */
static void
part_idle_fn(void *d)
{
	while (1) part_pool_wakeup();
}

struct part_data *
part_data_alloc(void)
{
	int i;
	struct part_data *d;

	for (i = 0; i < PART_MAX_TASKS; i++) {
		d = part__data + i;

		if (d->flag) continue;

		/* if this fails, someone else just alloced it! */
		if (!ps_cas(&d->flag, 0, 1)) continue;

		return d;
	}

	return NULL;
}

void
part_data_free(struct part_data *d)
{
	int f;

	if (!d) return;

	do {
		f = d->flag;
		assert(f);
	} while (!ps_cas(&d->flag, f, 0));
}
struct part_task *
part_task_alloc(part_task_type_t type)
{
	int i;
	struct part_task *t;

	for (i = 0; i < PART_MAX_TASKS; i++) {
		t = part_tasks + i;

		if (t->state != PART_TASK_S_FREED) continue;

		/* if this fails, someone else just alloced it! */
		if (!ps_cas(&t->state, PART_TASK_S_FREED, PART_TASK_S_ALLOCATED)) continue;

		return t;
	}

	return NULL;
}

void
part_task_free(struct part_task *t)
{
	part_task_state_t s = 0;

	if (!t) return;

	do {
		s = t->state;
		assert(s != PART_TASK_S_FREED);
	} while (!ps_cas(&t->state, s, PART_TASK_S_FREED));
}

unsigned
part_isready(void)
{ return (part_ready == NUM_CPU); }

void
part_init(void)
{
	int k;
	static volatile int is_first = NUM_CPU;
	struct sl_thd *it = NULL;
	struct sl_xcore_thd *xit = NULL;
	sched_param_t ip = _PART_IDLE_PRIO_PACK();
	static volatile int all_done = 0;

	ps_list_head_init(&part_thdpool_core[cos_cpuid()]);
	if (ps_cas(&is_first, NUM_CPU, cos_cpuid())) {
		for (k = 0; k < NUM_CPU; k++) deque_init_part(&part_dq_percore[k], PART_DEQUE_SZ);
		part_tasks = (struct part_task *)memmgr_heap_page_allocn(PART_MAX_PAGES);
		assert(part_tasks);
		memset(part_tasks, 0, PART_MAX_PAGES * PAGE_SIZE);

		part__data = (struct part_data *)memmgr_heap_page_allocn(PART_MAX_DATA_PAGES);
		assert(part__data);
		memset(part__data, 0, PART_MAX_DATA_PAGES * PAGE_SIZE);

#if defined(PART_ENABLE_NESTED)
		ps_list_head_init(&part_l_global);
		crt_lock_init(&part_l_lock);
#else
		memset(&main_task, 0, sizeof(main_task));
#endif
		in_main_parallel = 0;
	}
	
	for (k = 0; k < PART_MAX_CORE_THDS; k++) {
		struct sl_xcore_thd *x;
		struct sl_thd *t;
		sched_param_t p = _PART_PRIO_PACK();

		t = sl_thd_alloc(part_thd_fn, NULL);
		assert(t);

		sl_thd_param_set(t, p);

		x = sl_xcore_thd_lookup_init(sl_thd_thdid(t), cos_cpuid());
		assert(x);
	}

#ifdef PART_ENABLE_BLOCKING
	sl_cs_enter();
	/* 
	 * because it's fifo, all threads would go block 
	 * themselves up as there is no work yet
	 * eventually returning to this main thread on core-0, 
	 * and on all other cores, scheduler would be running!
	 */
	sl_cs_exit_schedule(); 
	it = sl_thd_alloc(part_idle_fn, NULL);
	assert(it);
	sl_thd_param_set(it, ip);
#endif

	ps_faa(&all_done, 1);
	while (ps_load(&all_done) != NUM_CPU) ;

	ps_faa(&part_ready, 1);
}
