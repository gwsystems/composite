#ifndef PART_TASK_H
#define PART_TASK_H

#include <sl.h>
#include <ps.h>
#include <ps_list.h>
#include <cos_types.h>

#define PART_THD(c, t) (cos_cpuid() << 16 | cos_thdid())
#define PART_CURR_THD  PART_THD(cos_cpuid(), cos_thdid()) 

#define PART_MAX_TASKS      256 
#define PART_MAX_DATA       128
#define PART_MAX_PAR_THDS   NUM_CPU
#define PART_MAX_THDS       128
#define PART_MAX_CORE_THDS  (PART_MAX_THDS/NUM_CPU)
#define PART_MAX_CHILD      16 
#define PART_MAX_WORKSHARES 16

typedef void (*part_fn_t)(void *);

typedef enum {
	PART_TASK_S_FREED,
	PART_TASK_S_ALLOCATED,
	PART_TASK_S_INITIALIZED,
	PART_TASK_S_RUNNING,
	PART_TASK_S_CHILD_WAIT, /* WAIT FOR CHILD TASKS */
	PART_TASK_S_SIBLING_WAIT, /* WAIT FOR SIBLING TASKS */
	PART_TASK_S_PARENT_WAIT, /* WAIT FOR PARENT TASK */
	PART_TASK_S_IN_BARRIER, /* WAIT FOR ALL OTHER THREADS */
} part_task_state_t;

typedef enum {
	PART_TASK_T_NONE,
	PART_TASK_T_WORKSHARE = 1, /* task to put in a shared fifo queue */
	PART_TASK_T_TASK,
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

struct part_data {
	int flag; /* 0 = not in use, 1 = in use */	
	char data[PART_MAX_DATA];
};

struct part_task {
	int id; /* only for debugging */
	part_task_state_t state;
	part_task_type_t  type;

	struct part_workshare ws[PART_MAX_WORKSHARES];
	struct part_closure   cs;

	unsigned nthds; /* number of threads for this task, 1 in case of non-workshare work */
	unsigned workers[PART_MAX_PAR_THDS]; /* threads sharing this work or thread doing this work! */
	int ws_off[PART_MAX_PAR_THDS]; /* progress of the workshares in each participating thread */
	unsigned master; /* coreid << 16 | thdid of the master */
	unsigned barrier_in, barrier_out, end;

	struct part_data *data_env; 
	struct part_task *parent;
	int nchildren;

	struct ps_list partask;
} CACHE_ALIGNED;

static inline void
part_task_init(struct part_task *t, part_task_type_t type, struct part_task *p, unsigned nthds, part_fn_t fn, void *data, struct part_data *d)
{
	static unsigned part_id_free = 0;
	int i, id = ps_faa(&part_id_free, 1);

	assert(type != PART_TASK_T_NONE);
	t->type = type;
	if (!ps_cas(&t->state, PART_TASK_S_ALLOCATED, PART_TASK_S_INITIALIZED)) assert(0);
	t->id = id;
	memset(t->ws, 0, sizeof(struct part_workshare) * PART_MAX_WORKSHARES);
	t->cs.fn = fn;
	t->cs.data = data;
	t->nthds = nthds;
	memset(t->workers, 0, sizeof(unsigned) * PART_MAX_PAR_THDS);
	t->master = PART_CURR_THD;
	/* if it's worksharing, current thread is the master and does take part in the par section */
	if (type == PART_TASK_T_WORKSHARE) t->workers[0] = t->master;
	for (i = 0; i < PART_MAX_PAR_THDS; i++) t->ws_off[i] = -1;
	t->barrier_in = t->barrier_out = t->end = 0;
	t->data_env = d;
	t->parent = p;
	t->nchildren = 0;

	ps_list_init(t, partask);
}

struct part_task *part_task_alloc(part_task_type_t);
void part_task_free(struct part_task *);
struct part_data *part_data_alloc(void);
void part_data_free(struct part_data *);

static inline int
part_task_add_child(struct part_task *t, struct part_task *c)
{
	int i;

	assert(t->state == PART_TASK_S_INITIALIZED);

	if (unlikely(!t || !c)) return -1;

	i = ps_faa(&t->nchildren, 1);
	assert(i < PART_MAX_CHILD);
	
	return i;
}

static inline void
part_task_remove_child(struct part_task *t, struct part_task *c)
{
	int i;

	if (unlikely(!t || !c)) return;
	assert(t->state == PART_TASK_S_INITIALIZED);

	i = ps_faa(&t->nchildren, -1);
	assert(i > 0);
}

static inline void
part_task_wait_children(struct part_task *t)
{
	assert(t->state == PART_TASK_S_INITIALIZED);
	while (ps_load(&t->nchildren) > 0) sl_thd_yield(0);

	assert(t->nchildren == 0);
}

static inline int
part_task_work_try(struct part_task *t)
{
	unsigned i;
        unsigned key = PART_CURR_THD;

	assert(t->state == PART_TASK_S_INITIALIZED);
	if (t->type == PART_TASK_T_TASK) {
		assert(t->nthds == 1);
	} else {
		assert(t->type == PART_TASK_T_WORKSHARE);
		assert(t->master != key && t->master == t->workers[0]);
		assert(t->nthds >= 1);
	}

	for (i = 0; i < t->nthds; i++)
	{
		unsigned w = ps_load(&t->workers[i]);

		if (w == key) return i;
		if (w) continue;

		if (likely(ps_cas(&t->workers[i], w, key))) return i;
	}

	return -1;
}

static inline int
part_task_work_thd_num(struct part_task *t)
{
	int i; 
	unsigned key = PART_CURR_THD;

	assert(t->state == PART_TASK_S_INITIALIZED);
	if (t->type == PART_TASK_T_TASK) {
		assert(t->nthds == 1);

		if (ps_load(&t->workers[0]) == key) return 0;

		return -1;
	}
	assert(t->type == PART_TASK_T_WORKSHARE);

	if (key == t->master) return 0;
	for (i = 1; i < (int)t->nthds; i++) {
		if (t->workers[i] == key) return i;
	}

	return -1;
}

static inline void
part_task_barrier(struct part_task *t)
{
	int tn = part_task_work_thd_num(t);
	unsigned cin = 0, cout = 0;

	assert(t->state == PART_TASK_S_INITIALIZED);
	assert(tn >= 0 && t->nthds >= 1);

	if (t->nthds == 1) {
		int i;

		assert(tn == 0 && t->barrier_in == 0);

		/* wait for all child tasks to complete, including explicit tasks */
		part_task_wait_children(t);

		return;
	}

	/* wait for all siblings to have seen the previous barrier */
	while (ps_load(&t->barrier_out) % t->nthds) sl_thd_yield(0);

	cin = ps_faa(&t->barrier_in, 1);
	if (cin % t->nthds == t->nthds - 1) {
		int i;

		/* wait for all child tasks to complete, including explicit tasks */
		part_task_wait_children(t);
	} else {
		/* wait for all sibling tasks to reach in barrier! */
		while (ps_load(&t->barrier_in) % t->nthds != 0) sl_thd_yield(0);
	}

	ps_faa(&t->barrier_out, 1);
}

#endif /* PART_TASK_H */
