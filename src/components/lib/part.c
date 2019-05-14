#include <cos_types.h>
#include <cos_component.h>
#include <part_task.h>
#include <part.h>
#include <../interface/capmgr/memmgr.h>

#include <sl.h>
#include <sl_xcore.h>

#define PART_MAX_PAGES ((PART_MAX_TASKS * sizeof(struct part_task)) / PAGE_SIZE)
#define PART_MAX_DATA_PAGES ((PART_MAX_TASKS * sizeof(struct part_data)) / PAGE_SIZE)

struct deque_part part_dq_percore[NUM_CPU];
//struct cirque_par parcq_global;
struct ps_list_head part_l_global;
static unsigned part_ready = 0;
struct crt_lock part_l_lock;
static struct part_task *part_tasks = NULL;
static struct part_data *part__data = NULL;
struct ps_list_head part_thdpool_core[NUM_CPU];

#define PART_DEQUE_SZ 64
#define _PART_PRIO 1
#define _PART_PRIO_PACK() sched_param_pack(SCHEDP_PRIO, _PART_PRIO)

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
	static int is_first = NUM_CPU, ds_init_done = 0;

	ps_list_head_init(&part_thdpool_core[cos_cpuid()]);
	if (!ps_cas(&is_first, NUM_CPU, cos_cpuid())) {
		while (!ps_load(&ds_init_done)) ;
	} else {
		for (k = 0; k < NUM_CPU; k++) deque_init_part(&part_dq_percore[k], PART_DEQUE_SZ);
		part_tasks = (struct part_task *)memmgr_heap_page_allocn(PART_MAX_PAGES);
		assert(part_tasks);


		part__data = (struct part_data *)memmgr_heap_page_allocn(PART_MAX_DATA_PAGES);
		assert(part__data);

		ps_list_head_init(&part_l_global);
		crt_lock_init(&part_l_lock);
		ps_faa(&ds_init_done, 1);
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

	ps_faa(&part_ready, 1);
}
