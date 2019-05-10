#ifndef PART_TASK_H
#define PART_TASK_H

#include <sl.h>
#include <ps.h>
#include <ps_list.h>
#include <cos_types.h>

#define PART_THD(c, t) (cos_cpuid() << 16 | cos_thdid())
#define PART_CURR_THD  PART_THD(cos_cpuid(), cos_thdid()) 

#define PART_MAX            4 
#define PART_MAX_CORE_THDS  4
#define PART_MAX_THDS       PART_MAX_CORE_THDS*NUM_CPU
#define PART_MAX_CHILD      PART_MAX
#define PART_MAX_WORKSHARES 16

typedef void (*part_fn_t)(void *);

typedef enum {
	PART_TASK_S_FREED,
	PART_TASK_S_ALLOCATED,
	PART_TASK_S_RUNNING,
	PART_TASK_S_CHILD_WAIT, /* WAIT FOR CHILD TASKS */
	PART_TASK_S_SIBLING_WAIT, /* WAIT FOR SIBLING TASKS */
	PART_TASK_S_PARENT_WAIT, /* WAIT FOR PARENT TASK */
	PART_TASK_S_IN_BARRIER, /* WAIT FOR ALL OTHER THREADS */
} part_task_state_t;

typedef enum {
	PART_TASK_T_WORKSHARE = 1, /* task to put in a shared fifo queue */
} part_task_type_t;

typedef enum {
	PART_WORKSHARE_NONE,
	PART_WORKSHARE_LOOP_STATIC,
	PART_WORKSHARE_LOOP_DYNAMIC,
	PART_WORKSHARE_LOOP_GUIDED,
	PART_WORKSHARE_LOOP_RUNTIME,
	PART_WORKSHARE_SECTIONS,
	PART_WORKSHARE_SINGLE,
} part_workshare_type_t;

struct part_workshare {
	part_workshare_type_t type;

	long chunk_sz;

	long st, end, inc;

	long next;

	unsigned worker_bmp;
};

struct part_closure {
	part_fn_t  fn;
	void     *data;
};

struct part_task {
	part_task_state_t state;
	part_task_type_t  type;

	struct part_workshare ws[PART_MAX_WORKSHARES];
	struct part_closure   cs;

	unsigned nthds; /* number of threads for this task, 1 in case of non-workshare work */
	unsigned workers[PART_MAX_THDS]; /* threads sharing this work or thread doing this work! */
	int ws_off[PART_MAX_THDS]; /* progress of the workshares in each participating thread */
	unsigned master; /* coreid << 16 | thdid of the master */
	unsigned barrier_in, barrier_out, end;

	/* TODO: parent to wait on all child tasks for taskwait synchronization! */
	struct part_task *parent;
	struct part_task *child[PART_MAX_CHILD];

	struct ps_list partask;
} CACHE_ALIGNED;

static inline void
part_task_init(struct part_task *t, part_task_type_t type, struct part_task *p, unsigned nthds, part_fn_t fn, void *data)
{
	int i;

	memset(t, 0, sizeof(struct part_task));

	ps_list_init(t, partask);
	t->type = type;
	t->state = PART_TASK_S_ALLOCATED;
	t->parent = p;
	t->nthds = nthds;

	t->master = PART_CURR_THD;
	t->cs.fn = fn;
	t->cs.data = data;

	for (i = 0; i < PART_MAX_THDS; i++) t->ws_off[i] = -1;

	/* if it's worksharing, current thread is the master and does take part in the par section */
	if (type == PART_TASK_T_WORKSHARE) t->workers[0] = t->master;
}

static inline int
part_task_add_child(struct part_task *t, struct part_task *c)
{
	int i;

	if (unlikely(!t || !c)) return -1;

	for (i = 0; i < PART_MAX_CHILD; i++) {
		if (likely(t->child[i] == 0 && ps_cas(&t->child[i], 0, (unsigned long)c))) return i;
	}

	return -1;
}

static inline void
part_task_remove_child(struct part_task *t, struct part_task *c)
{
	int i;

	if (unlikely(!t || !c)) return;

	for (i = 0; i < PART_MAX_CHILD; i++) {
		if (t->child[i] != c) continue;

		if (unlikely(!ps_cas(&t->child[i], (unsigned long)c, 0))) assert(0);
	}
}

static inline int
part_task_work_try(struct part_task *t)
{
	unsigned i = 0;
        unsigned key = PART_CURR_THD;

	if (t->type != PART_TASK_T_WORKSHARE) {
		assert(t->nthds == 1);
	} else {
		assert(t->master != key && t->master == t->workers[0]);
		assert(t->nthds >= 1);
	}

	for (; i < t->nthds; i++)
	{
		if (t->workers[i] == key) return i;
		if (t->workers[i]) continue;

		if (likely(ps_cas(&t->workers[i], 0, key))) return i;
	}

	return -1;
}

static inline int
part_task_work_thd_num(struct part_task *t)
{
	int i; 
	unsigned key = PART_CURR_THD;

	if (t->type != PART_TASK_T_WORKSHARE) assert(t->nthds == 1);

	if (key == t->master) return 0;
	for (i = 1; i < (int)t->nthds; i++) {
		if (t->workers[i] == key) return i;
	}

	return -1;
}

static inline void
part_task_barrier(struct part_task *t)
{
	struct sl_thd *ts = sl_thd_curr();
	int tn = part_task_work_thd_num(t);
	unsigned cin = 0, cout = 0;

	assert(tn >= 0 && t->nthds >= 1);

	if (t->nthds == 1) {
		assert(tn == 0 && t->barrier_in == 0);

		return;
	}

	/* wait for all siblings to have seen the previous barrier */
	while (ps_load(&t->barrier_out) % t->nthds) sl_thd_yield(0);

	cin = ps_faa(&t->barrier_in, 1);
	if (cin % t->nthds == t->nthds - 1) {
		int i;

		/* wait for all child tasks to complete, including explicit tasks */
		for (i = 0; i < PART_MAX_CHILD; i++) {
			while (ps_load(&t->child[i])) sl_thd_yield(0);
		}
	} else {
		/* wait for all sibling tasks to reach in barrier! */
		while (ps_load(&t->barrier_in) % t->nthds != 0) sl_thd_yield(0);
	}

	ps_faa(&t->barrier_out, 1);
}

#endif /* PART_TASK_H */
