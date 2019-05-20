#include <cos_types.h>
#include <cos_component.h>
#include <part_task.h>
#include <part.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <ps.h>
#include <ps_slab.h>

#include <sl.h>
#include <sl_xcore.h>

struct deque_part *part_dq_percore[NUM_CPU];
//struct cirque_par parcq_global;
static volatile unsigned part_ready = 0;
volatile int in_main_parallel;
#if defined(PART_ENABLE_NESTED)
struct crt_lock part_l_lock;
struct ps_list_head part_l_global;
#else
struct part_task main_task;
#endif
//static struct part_task *part_tasks = NULL;
//static struct part_data *part__data = NULL;
struct ps_list_head part_thdpool_core[NUM_CPU];

#define PART_DEQUE_SZ PART_MAX_TASKS
#define _PART_PRIO TCAP_PRIO_MAX
#define _PART_PRIO_PACK() sched_param_pack(SCHEDP_PRIO, _PART_PRIO)

#define _PART_IDLE_PRIO (_PART_PRIO+4)
#define _PART_IDLE_PRIO_PACK() sched_param_pack(SCHEDP_PRIO, _PART_IDLE_PRIO)

//struct ps_slab *
//ps_slab_memmgr_alloc(struct ps_mem *m, size_t sz, coreid_t coreid)
//{
//	PRINTC("%s:%d\n", __func__, __LINE__);
//	unsigned npages = round_up_to_page(sz) / PAGE_SIZE;
//	vaddr_t addr = memmgr_heap_page_allocn(npages);
//
//	assert(addr);
//	memset((void *)addr, 0, npages * PAGE_SIZE);
//	PRINTC("%s:%d\n", __func__, __LINE__);
//
//	return (struct ps_slab *)addr;
//}
//
//void
//ps_slab_memmgr_free(struct ps_mem *m, struct ps_slab *s, size_t sz, coreid_t coreid)
//{
//	/* do nothing */
//}

/* this? */
//PS_SLAB_CREATE_AFNS(parttask, sizeof(struct part_task), PART_TASKS_MAX_SZ, 0, ps_slab_memmgr_alloc, ps_slab_memmgr_free);
//PS_SLAB_CREATE_AFNS(partdata, sizeof(struct part_data), PART_DATA_MAX_SZ, 0, ps_slab_memmgr_alloc, ps_slab_memmgr_free);
/* or this. */
//PS_SLAB_CREATE(parttask, sizeof(struct part_task), PART_TASKS_MAX_SZ)
//PS_SLAB_CREATE(partdata, sizeof(struct part_data), PART_DATA_MAX_SZ)

/* for task pool, per core list. tasks in pool can migrate cores */
struct parttask_head {
	struct part_task *head;
};

static inline void
parttask_store_init(struct parttask_head *h)
{
	h->head = NULL;
}

static inline void
parttask_store_add(struct parttask_head *h, struct part_task *l)
{
	struct part_task *n;
	l->next_free = NULL;

	assert(h);
	do {
		n = ps_load(&h->head);
		l->next_free = n;
	} while (!ps_cas(&h->head, (unsigned long)n, (unsigned long)l)); 
}

static inline struct part_task *
parttask_store_dequeue(struct parttask_head *h)
{
	struct part_task *l = NULL;

	do {
		l = ps_load(&h->head);
		if (unlikely(!l)) return NULL;
	} while (!ps_cas(&h->head, (unsigned long)l, (unsigned long)l->next_free));

	l->next_free = NULL;

	return l;
}

/* for task data, per core pool - task data could migrate pools. */
struct partdata_head {
	struct part_data *head;
};

static inline void
partdata_store_init(struct partdata_head *h)
{
	h->head = NULL;
}

static inline void
partdata_store_add(struct partdata_head *h, struct part_data *l)
{
	struct part_data *n = NULL;
	l->next_free = NULL;

	assert(h);
	do {
		n = ps_load(&h->head);

		l->next_free = n;
	} while (!ps_cas(&h->head, (unsigned long)n, (unsigned long)l)); 
}

static inline struct part_data *
partdata_store_dequeue(struct partdata_head *h)
{
	struct part_data *l = NULL;

	do {
		l = ps_load(&h->head);
		if (unlikely(!l)) return NULL;
	} while (!ps_cas(&h->head, (unsigned long)l, (unsigned long)l->next_free));

	l->next_free = NULL;

	return l;
}

/* end treiber stacks */
#define PART_TASKS_MAX_SZ round_up_to_page(PART_MAX_TASKS * sizeof(struct part_task))
#define PART_MAX_PAGES (PART_TASKS_MAX_SZ / PAGE_SIZE)
#define PART_DATA_MAX_SZ round_up_to_page(PART_MAX_TASKS * sizeof(struct part_data))
#define PART_MAX_DATA_PAGES (PART_DATA_MAX_SZ / PAGE_SIZE)
#define PART_DEQUE_MAX_SZ round_up_to_page(sizeof(struct deque_part))
#define PART_DEQUE_MAX_PAGES (PART_DEQUE_MAX_SZ / PAGE_SIZE)

struct partdata_head pd_head[NUM_CPU];

static inline void
partdata_store_init_all(vaddr_t mem)
{
	int i;

	for (i = 0; i < NUM_CPU; i++) {
		int j;
		struct part_data *st = (struct part_data *)(mem + (PART_DATA_MAX_SZ * i));

		partdata_store_init(&pd_head[i]);
		
		for (j = 0; j < PART_MAX_TASKS; j++) partdata_store_add(&pd_head[i], st + j);
	}
}

struct parttask_head pt_head[NUM_CPU];

static inline void
parttask_store_init_all(vaddr_t mem)
{
	int i;

	for (i = 0; i < NUM_CPU; i++) {
		int j;
		struct part_task *st = (struct part_task *)(mem + (PART_TASKS_MAX_SZ * i));

		parttask_store_init(&pt_head[i]);
		
		for (j = 0; j < PART_MAX_TASKS; j++) parttask_store_add(&pt_head[i], st + j);
	}
}

/* idle thread to wakeup when there is nothing to do on this core! */
static void
part_idle_fn(void *d)
{
	struct sl_thd *sched = sl__globals_core()->sched_thd, *curr = sl_thd_curr();

	while (1) {
		/*
		 * TODO: threads could be woken up even if there is no work!
		 */
		if (likely(ps_load(&in_main_parallel))) part_pool_wakeup();
		sl_thd_yield_thd(sched);
	}
}

struct part_data *
part_data_alloc(void)
{
	struct part_data *d = partdata_store_dequeue(&pd_head[cos_cpuid()]);

	if (!d) return d;
	if (!ps_cas(&d->flag, 0, 1)) assert(0);

	return d;
//	int i;
//	struct part_data *d = ps_slab_alloc_partdata();
//
//	if (!ps_cas(&d->flag, 0, 1)) assert(0);
//
//	return d;
//	for (i = 0; i < PART_MAX_TASKS; i++) {
//		d = part__data + i;
//
//		if (d->flag) continue;
//
//		/* if this fails, someone else just alloced it! */
//		if (!ps_cas(&d->flag, 0, 1)) continue;
//
//		return d;
//	}
//
//	return NULL;
}

void
part_data_free(struct part_data *d)
{
	if (!ps_cas(&d->flag, 1, 0)) assert(0);

	partdata_store_add(&pd_head[cos_cpuid()], d);
//	ps_slab_free_partdata(d);
//	int f;
//
//	if (!d) return;
//
//	do {
//		f = d->flag;
//		assert(f);
//	} while (!ps_cas(&d->flag, f, 0));
}
struct part_task *
part_task_alloc(part_task_type_t type)
{
	struct part_task *t = parttask_store_dequeue(&pt_head[cos_cpuid()]);

	if (!t) return t;

	/* use upcas ? */
	if (!ps_cas(&t->state, PART_TASK_S_FREED, PART_TASK_S_ALLOCATED)) assert(0);

	return t;
//	struct part_task *t = ps_slab_alloc_parttask();
//
//	if (!ps_cas(&t->state, PART_TASK_S_FREED, PART_TASK_S_ALLOCATED)) assert(0);
//
//	return t;
//	int i;
//	struct part_task *t;
//
//	for (i = 0; i < PART_MAX_TASKS; i++) {
//		t = part_tasks + i;
//
//		if (ps_load(&t->state) != PART_TASK_S_FREED) continue;
//
//		/* if this fails, someone else just alloced it! */
//		if (!ps_cas(&t->state, PART_TASK_S_FREED, PART_TASK_S_ALLOCATED)) continue;
//
//		return t;
//	}
//
//	return NULL;
}

void
part_task_free(struct part_task *t)
{
	if (!ps_cas(&t->state, PART_TASK_S_INITIALIZED, PART_TASK_S_FREED)) assert(0);

	parttask_store_add(&pt_head[cos_cpuid()], t);
//	ps_slab_free_parttask(t);
//	part_task_state_t s = 0;
//
//	if (!t) return;
//
//	do {
//		s = ps_load(&t->state);
//		if (s != PART_TASK_S_INITIALIZED) return;
//	} while (!ps_cas(&t->state, s, PART_TASK_S_FREED));
}

unsigned
part_isready(void)
{ return (part_ready == NUM_CPU); }

void
part_init(void)
{
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	int k;
	static volatile int is_first = NUM_CPU;
	struct sl_thd *it = NULL;
	struct sl_xcore_thd *xit = NULL;
	sched_param_t ip = _PART_IDLE_PRIO_PACK();
	static volatile int all_done = 0;

	ps_list_head_init(&part_thdpool_core[cos_cpuid()]);
	if (ps_cas(&is_first, NUM_CPU, cos_cpuid())) {
		vaddr_t ptmem = 0, pdmem = 0;

		for (k = 0; k < NUM_CPU; k++) {
			part_dq_percore[k] = (struct deque_part *)cos_page_bump_allocn(ci, PART_DEQUE_MAX_SZ);
			assert(part_dq_percore[k]);
			deque_init_part(part_dq_percore[k], PART_DEQUE_SZ);
		}
		ptmem = (vaddr_t)cos_page_bump_allocn(ci, PART_TASKS_MAX_SZ * NUM_CPU);
		assert(ptmem);
		memset((void *)ptmem, 0, PART_MAX_PAGES * PAGE_SIZE * NUM_CPU);

		pdmem = (vaddr_t)cos_page_bump_allocn(ci, PART_DATA_MAX_SZ * NUM_CPU);
		assert(pdmem);
		memset((void *)pdmem, 0, PART_MAX_DATA_PAGES * PAGE_SIZE * NUM_CPU);

		partdata_store_init_all(pdmem);
		parttask_store_init_all(ptmem);
//		ps_slab_init_parttask();
//		ps_slab_init_partdata();

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
