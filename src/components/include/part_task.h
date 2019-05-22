#ifndef PART_TASK_H
#define PART_TASK_H

#include <sl.h>
#include <ps.h>
#include <ps_list.h>
#include <cos_types.h>

#define PART_THD(c, t) ((unsigned)(cos_cpuid() << 16 | cos_thdid()))
#define PART_CURR_THD  PART_THD(cos_cpuid(), cos_thdid()) 
#define PART_THD_COREID(t) (t >> 16)
#define PART_THD_THDID(t)  ((t << 16) >> 16)

#define PART_MAX_TASKS      2048
#define PART_MAX_DATA       2048
#define PART_MAX_PAR_THDS   NUM_CPU
#define PART_MAX_CORE_THDS  48
#define PART_MAX_THDS       512
#define PART_MAX_CHILD      1024
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
	struct part_data *next_free; /* for explicit data allocation/free */
	char data[PART_MAX_DATA];
};

struct part_task {
	int id; /* only for debugging */
	part_task_state_t state;
	part_task_type_t  type;

	struct part_workshare ws[PART_MAX_WORKSHARES];
	struct part_closure   cs;

	unsigned nthds; /* number of threads for this task, 1 in case of non-workshare work */
	unsigned nworkers;
	unsigned workers[PART_MAX_PAR_THDS]; /* threads sharing this work or thread doing this work! */
	int ws_off[PART_MAX_PAR_THDS]; /* progress of the workshares in each participating thread */
	unsigned master; /* coreid << 16 | thdid of the master */
	unsigned end, barrier, barrier_epoch;

	struct part_data *data_env; 
	struct part_task *parent;
	/* in data-parallel task, each thread waits for its children. */
	int nchildren[PART_MAX_PAR_THDS];

	struct ps_list partask;
	struct part_task *next_free; /* for explicit task allocation/free */
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
	t->nworkers = 0;
	memset(t->workers, 0, sizeof(unsigned) * PART_MAX_PAR_THDS);
	t->master = PART_CURR_THD;
	/* if it's worksharing, current thread is the master and does take part in the par section */
	if (type == PART_TASK_T_WORKSHARE) {
		t->nworkers = 1;
		t->workers[0] = t->master;
	}
	for (i = 0; i < PART_MAX_PAR_THDS; i++) t->ws_off[i] = -1;
	t->barrier = t->nthds;
	t->end = t->barrier_epoch = 0;
	t->data_env = d;
	t->parent = p;
	memset(t->nchildren, 0, sizeof(int) * PART_MAX_PAR_THDS);

	ps_list_init(t, partask);
}

struct part_task *part_task_alloc(part_task_type_t);
void part_task_free(struct part_task *);
struct part_data *part_data_alloc(void);
void part_data_free(struct part_data *);

static inline int
part_task_work_try(struct part_task *t)
{
	int i = 0;
        unsigned key = PART_CURR_THD;

	assert(t->state == PART_TASK_S_INITIALIZED);
	if (t->type == PART_TASK_T_TASK) {
		assert(t->nthds == 1);
	} else {
		assert(t->type == PART_TASK_T_WORKSHARE);
		assert(t->master != key && t->master == t->workers[0]);
		assert(t->nthds >= 1);
	}

	/* task was finished! */
	if (unlikely(ps_load(&t->end) == t->nthds)) return -1;
	/* if you can work with this task */
	i = ps_faa(&t->nworkers, 1);
	if (unlikely(i >= (int)t->nthds)) return -1;

	t->workers[i] = key;

	return i;
}

static inline int
part_task_work_thd_num(struct part_task *t, unsigned core_thd)
{
	int i; 
	unsigned key = core_thd;

	assert(t);

	assert(t->state == PART_TASK_S_INITIALIZED);
	if (likely(t->type == PART_TASK_T_TASK)) {
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
part_thd_wakeup(unsigned thd)
{
	thdid_t t = PART_THD_THDID(thd);
	cpuid_t c = PART_THD_COREID(thd);

	assert(c >= 0 && c < NUM_CPU);
	assert(t < MAX_NUM_THREADS);

	if (thd == PART_CURR_THD) return;
	if (c != cos_cpuid()) sl_xcore_thd_wakeup_tid(t, c);
	else                  sl_thd_wakeup(t);
}

static inline void
part_master_wakeup(struct part_task *t)
{
	assert(t->type == PART_TASK_T_WORKSHARE);
	assert(t->state == PART_TASK_S_INITIALIZED);
	assert(t->nthds > 1);
	assert(t->master && t->master != PART_CURR_THD);

	part_thd_wakeup(t->master);
}

static inline void
part_peer_wakeup(struct part_task *t)
{
	unsigned i;

	assert(t->type == PART_TASK_T_WORKSHARE);
	assert(t->state == PART_TASK_S_INITIALIZED);
	assert(t->nthds > 1);
	assert(t->master == PART_CURR_THD);

	for (i = 1; i < t->nthds; i++) part_thd_wakeup(t->workers[i]);
}

static inline int
part_task_add_child(struct part_task *t, struct part_task *c)
{
	int i;
	int num = part_task_work_thd_num(t, PART_CURR_THD);

	assert(num >= 0);
	assert(t->state == PART_TASK_S_INITIALIZED);

	if (unlikely(!t || !c)) return -1;

	i = ps_faa(&t->nchildren[num], 1);
	assert(i < PART_MAX_CHILD);
	
	return i;
}

static inline void
part_task_remove_child(struct part_task *c)
{
	struct part_task *p = c->parent;
	unsigned wkup;
	int i, num;

	if (unlikely(!p)) return;
	assert(c->state == PART_TASK_S_INITIALIZED);

	if (c->type == PART_TASK_T_TASK) wkup = c->master;
	else                             wkup = p->master;

	num = part_task_work_thd_num(p, wkup);
	assert(num >= 0);

	assert(p->nchildren[num] != 0);
	i = ps_faa(&p->nchildren[num], -1);
	assert(i > 0);

	/* only the last child to wake up the parent */
	if (i == 1) part_thd_wakeup(wkup);
}

static inline void
part_task_wait_children(struct part_task *t)
{
	int num = part_task_work_thd_num(t, PART_CURR_THD);

	assert(num >= 0);
	assert(t->state == PART_TASK_S_INITIALIZED);

	if (ps_load(&(t->nchildren[num])) > 0) sl_thd_block(0);
}

#endif /* PART_TASK_H */
