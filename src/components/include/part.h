#ifndef PART_H
#define PART_H

#include <part_task.h>
#include <ps_list.h>
#include <deque.h>

#include <sl.h>
//#include <cirque.h>

DEQUE_PROTOTYPE(part, struct part_task *);
//CIRQUE_PROTOTYPE(part, struct part_task);

extern struct deque_part part_dq_percore[];
//extern struct cirque_par parcq_global;
extern struct ps_list_head part_l_global;

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
part_deque_pop(struct part_task *t)
{
	int ret;

	sl_cs_enter();
	ret = deque_pop_part(part_deque_curr(), &t);
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
		if (c == (unsigned)cos_cpuid()) c = (c + 1) % NUM_CPU;

		t = part_deque_steal(c);
		if (t) return t;
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

	ps_list_head_append(part_list(), t, partask);
}

static inline void
part_list_remove(struct part_task *t)
{
	assert(t->type == PART_TASK_T_WORKSHARE);
	assert(!ps_list_singleton(t, partask));

	ps_list_rem(t, partask);
}

static inline struct part_task *
part_list_peek(void)
{
	struct part_task *t = NULL;

	if (ps_list_head_empty(part_list())) return NULL;
	/* not great! traversing from the first element always! */
	/* TODO: perhaps traverse from the current task? */
	ps_list_foreach(part_list(), t, partask) {
		int i;

		assert(t);

		assert(t->type == PART_TASK_T_WORKSHARE);
		/* coz, master thread adds to list the implicit task and doesn't defer it */
		i = part_task_work_try(t);
		assert(i != 0);

		if (i > 0) return t; 
	}

	return NULL;
}

void part_init(void);

unsigned part_isready(void);

static inline void
part_thd_fn(void *d)
{
	struct sl_thd *curr = sl_thd_curr();

	while (!part_isready()) sl_thd_yield(0);
	while (ps_list_head_empty(part_list())) sl_thd_yield(0);

	while (1) {
		struct part_task *t = NULL;
		int ret;
		int thdnum = 0;
		unsigned thd = cos_cpuid() << 16 | cos_thdid();

		/* FIXME: nested parallel needs love! */
		t = part_list_peek();
		if (t) goto found;

single:
		ret = part_deque_pop(t);
		if (ret == 0) {
			assert(t->type != PART_TASK_T_WORKSHARE);

			goto found;
		}

		if (ret == -EAGAIN) goto single;

		t = part_deque_steal_any();
		if (!t) {
			sl_thd_yield(0);
			continue;
		}
		assert(t->type != PART_TASK_T_WORKSHARE);
found:
		thdnum = part_task_work_try(t);
		if (thdnum < 0) continue;
		if (t->type != PART_TASK_T_WORKSHARE) assert(thdnum == 0);
		curr->part_context = (void *)t;

		t->cs.fn(t->cs.data);

		if (t->type != PART_TASK_T_WORKSHARE) continue;

		part_task_barrier(t);
	}

	sl_thd_exit();
}

#endif /* PART_H */
