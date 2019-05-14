#ifndef PART_H
#define PART_H

#include <part_task.h>
#include <ps_list.h>
#include <deque.h>
#include <crt_lock.h>

#include <sl.h>

#define PART_NESTED 0 /* 0 - disabled, 1 - enabled */
//#include <cirque.h>

DEQUE_PROTOTYPE(part, struct part_task *);
//CIRQUE_PROTOTYPE(part, struct part_task);

extern struct deque_part part_dq_percore[];
//extern struct cirque_par parcq_global;
/* FIXME: use stacklist or another stack like data structure? */
extern struct ps_list_head part_l_global;
extern struct crt_lock     part_l_lock;

static inline struct deque_part *
part_deque_curr(void)
{
	return &part_dq_percore[cos_cpuid()];
}

static inline struct deque_part *
part_deque_core(cpuid_t c)
{
	assert(c < NUM_CPU);

	return &part_dq_percore[c];
}

//static inline struct cirque_par *
//part_cirque(void)
//{
//	return &parcq_global;
//}

static inline struct ps_list_head *
part_list(void)
{
	return &part_l_global;
}

static inline int 
part_deque_push(struct part_task *t)
{
	int ret;

	sl_cs_enter();
	ret = deque_push_part(part_deque_curr(), &t);
	sl_cs_exit();

	return ret;
}

static inline int
part_deque_pop(struct part_task **t)
{
	int ret;

	sl_cs_enter();
	ret = deque_pop_part(part_deque_curr(), t);
	sl_cs_exit();

	return ret;
}

static inline struct part_task * 
part_deque_steal(cpuid_t core)
{
	int ret;
	struct part_task *t = NULL;

	ret = deque_steal_part(part_deque_core(core), &t);
	if (ret) return NULL;

	return t;
}

static inline struct part_task * 
part_deque_steal_any(void)
{
	unsigned i = 0, c = (unsigned)(ps_tsc() % NUM_CPU);

	do {
		struct part_task *t = NULL;

		i ++;
		if (unlikely(c == (unsigned)cos_cpuid())) c = (c + 1) % NUM_CPU;

		t = part_deque_steal(c);
		if (likely(t)) return t;
	} while (i < NUM_CPU);

	return NULL;
}

///* ds memory in a circular queue */
//static inline struct part_task * 
//part_cirque_alloc(void)
//{
//	return cirque_allocptr_par(part_cirque());
//}
//
//static inline void
//part_cirque_free(void)
//{
//	cirque_freeptr_par(part_cirque());
//}
//
//static inline struct part_task * 
//part_cirque_peek(void)
//{
//	return cirque_peekptr_par(part_cirque());
//}

/* TODO: lock for shared list! */
static inline void
part_list_append(struct part_task *t)
{
	assert(ps_list_singleton(t, partask));
	assert(t->type == PART_TASK_T_WORKSHARE);

	crt_lock_take(&part_l_lock);
	ps_list_head_append(part_list(), t, partask);
	crt_lock_release(&part_l_lock);
}

static inline void
part_list_remove(struct part_task *t)
{
	assert(t->type == PART_TASK_T_WORKSHARE);
	assert(!ps_list_singleton(t, partask));

	crt_lock_take(&part_l_lock);
	ps_list_rem(t, partask);
	crt_lock_release(&part_l_lock);
}

static inline struct part_task *
part_list_peek(void)
{
	struct part_task *t = NULL;
	int found = 0;

	crt_lock_take(&part_l_lock);
	if (unlikely(ps_list_head_empty(part_list()))) goto done;
	/* not great! traversing from the first element always! */
	/* TODO: perhaps traverse from the current task? */
	ps_list_foreach(part_list(), t, partask) {
		int i;

		assert(t);
		assert(t->type == PART_TASK_T_WORKSHARE);
		/* coz, master thread adds to list the implicit task and doesn't defer it */
		i = part_task_work_try(t);
		assert(i != 0);

		if (likely(i > 0 && !ps_load(&t->end))) {
			found = 1;
			break;
		}
	}

done:
	crt_lock_release(&part_l_lock);

	if (unlikely(!found)) return NULL;

	return t;
}

void part_init(void);

unsigned part_isready(void);

/* a part_task.h api but uses part_list_remove in the master thread, so here! */
static inline void
part_task_end(struct part_task *t)
{
	struct sl_thd *ts = sl_thd_curr();
	int tn = part_task_work_thd_num(t);

	assert(tn >= 0 && t->nthds >= 1);
	assert(ts->part_context == (void *)t);
	if (t->nthds == 1) {
		int i;

		assert(tn == 0);
		part_task_wait_children(t);
		ps_faa(&t->end, 1);
		part_task_remove_child(t->parent, t);
		if (t->type == PART_TASK_T_WORKSHARE) {
			assert(t->workers[tn] == t->master);
			ts->part_context = t->parent;
		}

		return;
	}
	part_task_barrier(t);

	if (tn == 0) {
		if (t->type == PART_TASK_T_WORKSHARE) part_list_remove(t);
		ts->part_context = t->parent;
		part_task_remove_child(t->parent, t);
		ps_faa(&t->end, 1);
	} else {
		ps_faa(&t->end, 1);
		while (ps_load(&t->end) != t->nthds) sl_thd_yield(0);

		ts->part_context = NULL;
	}
}


static inline void
part_thd_fn(void *d)
{
	struct sl_thd *curr = sl_thd_curr();

	/* parallel runtime not ready? */
	while (unlikely(!part_isready())) sl_thd_yield(0);

	while (1) {
		struct part_task *t = NULL;
		int ret;
		int thdnum = -1;
		unsigned thd = cos_cpuid() << 16 | cos_thdid();

		/* FIXME: nested parallel needs love! */
		t = part_list_peek();
		if (likely(t)) {
			thdnum = part_task_work_try(t);
			if (thdnum >= 0) goto found;
		}

single:
		ret = part_deque_pop(&t);
		if (likely(ret == 0)) {
			assert(t && t->type != PART_TASK_T_WORKSHARE);
			thdnum = part_task_work_try(t);
			if (thdnum == 0) goto found;
		}

		if (unlikely(ret == -EAGAIN)) goto single;

		t = part_deque_steal_any();
		if (unlikely(!t)) {
			sl_thd_yield(0);
			continue;
		}
		assert(t->type != PART_TASK_T_WORKSHARE);
found:
		if (unlikely(thdnum < 0)) thdnum = part_task_work_try(t);
		if (unlikely(thdnum < 0)) continue;
		if (t->type != PART_TASK_T_WORKSHARE) assert(thdnum == 0);
		curr->part_context = (void *)t;

		t->cs.fn(t->cs.data);

		part_task_end(t);
		/* free the explicit task! */
		if (t->type != PART_TASK_T_WORKSHARE) {
			struct part_data *d = t->data_env;

			part_task_free(t);
			part_data_free(d);
		}
		curr->part_context = NULL;
	}

	sl_thd_exit();
}

#endif /* PART_H */
