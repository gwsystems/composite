#ifndef PART_H
#define PART_H

#include <part_task.h>
#include <ps_list.h>
#include <deque.h>
#include <crt_lock.h>

#include <sl.h>
#include <sl_xcore.h>

#undef PART_ENABLE_NESTED
#define PART_ENABLE_BLOCKING
//#include <cirque.h>

DEQUE_PROTOTYPE(part, struct part_task *);
//CIRQUE_PROTOTYPE(part, struct part_task);

extern struct deque_part *part_dq_percore[];
//extern struct cirque_par parcq_global;
/* FIXME: use stacklist or another stack like data structure? */
extern struct ps_list_head part_thdpool_core[];
extern volatile int in_main_parallel;
#if defined(PART_ENABLE_NESTED)
extern struct ps_list_head part_l_global;
extern struct crt_lock     part_l_lock;
#else 
extern struct part_task main_task;
#endif

static inline struct deque_part *
part_deque_curr(void)
{
	return part_dq_percore[cos_cpuid()];
}

static inline struct deque_part *
part_deque_core(cpuid_t c)
{
	assert(c < NUM_CPU);

	return part_dq_percore[c];
}

static inline struct ps_list_head *
part_thdpool_curr(void)
{
	return &part_thdpool_core[cos_cpuid()];
}

//static inline struct cirque_par *
//part_cirque(void)
//{
//	return &parcq_global;
//}

#if defined(PART_ENABLE_NESTED)
static inline struct ps_list_head *
part_list(void)
{
	return &part_l_global;
}
#endif

static inline int 
part_deque_push(struct part_task *t)
{
	int ret;

	assert(t->type == PART_TASK_T_TASK);
	sl_cs_enter();
	ret = deque_push_part(part_deque_curr(), &t);
	sl_cs_exit();

	return ret;
}

static inline int
part_deque_pop(struct part_task **t)
{
	int ret;

	*t = NULL;
	sl_cs_enter();
	ret = deque_pop_part(part_deque_curr(), t);
	sl_cs_exit();
	if (unlikely(ret)) *t = NULL;

	if (unlikely(*t && (*t)->type != PART_TASK_T_TASK)) { *t = NULL; ret = -EAGAIN; }

	return ret;
}

static inline struct part_task * 
part_deque_steal(cpuid_t core)
{
#if NUM_CPU > 1
	int ret;
	struct part_task *t = NULL;

	ret = deque_steal_part(part_deque_core(core), &t);
	if (unlikely(ret)) return NULL;
	assert(t->type == PART_TASK_T_TASK);

	return t;
#else
	return NULL;
#endif
}

static inline struct part_task * 
part_deque_steal_any(void)
{
#if NUM_CPU > 1
	unsigned i = 0, c = (unsigned)(ps_tsc() % NUM_CPU);

	do {
		struct part_task *t = NULL;

		i ++;
		if (unlikely(c == (unsigned)cos_cpuid())) c = (c + 1) % NUM_CPU;

		t = part_deque_steal(c);
		if (likely(t)) return t;
	} while (i < NUM_CPU);
#endif
	return NULL;
}

static inline void
part_pool_wakeup(void)
{
#ifdef PART_ENABLE_BLOCKING
	struct sl_thd *t = NULL;
	int i;

	/* we're still not in main parallel, so don't try to wakeup any threads yet! */
	if (!ps_load(&in_main_parallel)) return;

	sl_cs_enter();
	if (unlikely(ps_list_head_empty(part_thdpool_curr()))) goto done;

	t = ps_list_head_first(part_thdpool_curr(), struct sl_thd, partlist);
	/* removal from the list is taken care in mod_part_fifo */
	if (t == sl_thd_curr()) goto done;
	sl_thd_wakeup_no_cs(t);
done:
	sl_cs_exit();
#endif
}

static inline void
part_pool_block(void)
{
#ifdef PART_ENABLE_BLOCKING
	struct sl_thd *t = sl_thd_curr();

	/* very much a replica of sl_thd_block + adding to thread pool in part */
	sl_cs_enter();
	if (sl_thd_block_no_cs(t, SL_THD_BLOCKED, 0)) {
		sl_cs_exit();
		return;
	}
	if (ps_list_singleton(t, partlist)) ps_list_head_append(part_thdpool_curr(), t, partlist);
	sl_cs_exit();
	sl_thd_block(0);
	assert(sl_thd_is_runnable(t));
#else
	sl_thd_yield(0);
#endif
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
	unsigned i;
	int in_nest = 0;

	assert(t->type == PART_TASK_T_WORKSHARE);

#if defined(PART_ENABLE_NESTED)
	assert(ps_list_singleton(t, partask));
	/* 
	 * this is not required to be in a cs. 
	 * if multiple appends are called, simultaneously, we at least
	 * have the main outermost parallel block running!.
	 */
	if (likely(!ps_list_head_empty(part_list()))) in_nest = 1;
	/* so other threads can work on this! */
	if (t->nthds > 1) { 
		crt_lock_take(&part_l_lock);
		ps_list_head_append(part_list(), t, partask);
		crt_lock_release(&part_l_lock);
	}
#else
	if (t != &main_task) {
		/* without nesting, all child parallel blocks are run just be the encountering threads -master threads */
		assert(t->nthds == 1); 
		assert(ps_load(&in_main_parallel));

		return;
	}
	assert(ps_load(&in_main_parallel) == 0);
#endif
	/* 
	 * wake up as many threads on this core! 
	 * some may not get work if other cores pull work before they get to it.
	 */
	for (i = 1; i < t->nthds; i++) part_pool_wakeup();

	/* if this is the first time in a parallel, make everyone know */
	if (likely(!in_nest)) ps_faa(&in_main_parallel, 1);
}

static inline void
part_list_remove(struct part_task *t)
{
	int in_nest = 0;

	assert(t->type == PART_TASK_T_WORKSHARE);
	assert(t->nthds > 1);
#if defined(PART_ENABLE_NESTED)
	assert(!ps_list_singleton(t, partask));

	crt_lock_take(&part_l_lock);
	ps_list_rem(t, partask);
	if (unlikely(!ps_list_head_empty(part_list()))) in_nest = 1;
	crt_lock_release(&part_l_lock);
#else
	/* only called for the other parallel region */
	assert(ps_load(&in_main_parallel));
	if (t != &main_task) return;
#endif

	if (likely(!in_nest)) ps_faa(&in_main_parallel, -1);
}

static inline struct part_task *
part_list_peek(void)
{
	/* there should at least be the outer parallel block for other threads to peek! */
	if (!ps_load(&in_main_parallel)) return NULL;

#if defined(PART_ENABLE_NESTED)
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
#else
	int i;

	assert(main_task.type == PART_TASK_T_WORKSHARE);
	i = part_task_work_try(&main_task);
	assert(i != 0);

	if (likely(i > 0 && ps_load(&main_task.end) != main_task.nthds)) return &main_task;

	return NULL;
#endif
}

void part_init(void);

unsigned part_isready(void);

static inline void
part_task_barrier(struct part_task *t, int is_end)
{
	struct sl_thd *ts = sl_thd_curr();
	unsigned cbc = 0, cbep = 0;
	int is_master = t->master == PART_CURR_THD ? 1 : 0;

	assert(t->type != PART_TASK_T_NONE);
	assert(t->state == PART_TASK_S_INITIALIZED);
	assert(t->nthds >= 1);

	part_task_wait_children(t);

	if (t->nthds == 1) {
		struct part_data *d;

		if (unlikely(!is_end)) return;

		ps_faa(&t->end, 1);
		/* remove myself from my parent. */
		part_task_remove_child(t);
		if (t->type == PART_TASK_T_WORKSHARE) {
			assert(is_master);
			ts->part_context = t->parent;

			return;
		}

		ts->part_context = NULL;
		d = t->data_env;

		part_task_free(t);
		part_data_free(d);

		return;
	}

	assert(t->type == PART_TASK_T_WORKSHARE);

	cbep = ps_load(&t->barrier_epoch);
	cbc = ps_faa(&t->barrier, -1);
	if (cbc > 1) {
		sl_thd_block(0);
	} else {
		if (ps_cas(&t->barrier, 0, t->nthds)) ps_faa(&t->barrier_epoch, 1);
		if (is_master) part_peer_wakeup(t);
		else part_master_wakeup(t);
	}
	assert(ps_load(&t->barrier_epoch) == cbep + 1);

	if (!is_end) return;
	ps_faa(&t->end, 1);

	if (is_master) {
		while (ps_load(&t->end) != t->nthds) sl_thd_block(0);
		part_task_remove_child(t);
		part_list_remove(t);
		ts->part_context = t->parent;
	} else {
		part_master_wakeup(t);
		ts->part_context = NULL;
	}
}

static inline void
part_task_end(struct part_task *t)
{ part_task_barrier(t, 1); }

static inline void
part_thd_fn(void *d)
{
	struct sl_thd *curr = sl_thd_curr();

	/* parallel runtime not ready? */
	/* if (unlikely(!part_isready())) part_pool_block(); */
	/* not in the main parallel block? */

	while (1) {
		struct part_task *t = NULL;
		int ret;

		if (!ps_load(&in_main_parallel)) part_pool_block();

		/* FIXME: nested parallel needs love! */
		t = part_list_peek();
		if (likely(t)) goto found;

single:
		ret = part_deque_pop(&t);
		if (likely(ret == 0)) {
			int thdnum = -1;

			assert(t && t->type == PART_TASK_T_TASK);
			thdnum = part_task_work_try(t);
			assert(thdnum == 0);
			goto found;
		}

		if (unlikely(ret == -EAGAIN)) goto single;

		t = part_deque_steal_any();
		if (unlikely(!t)) {
			part_pool_block();

			continue;
		} else {
			int thdnum = -1;

			assert(t->type == PART_TASK_T_TASK);
			thdnum = part_task_work_try(t);
			if (thdnum < 0) continue;
			assert(thdnum == 0);
		}

found:
		assert(t);
		curr->part_context = (void *)t;

		t->cs.fn(t->cs.data);

		part_task_end(t);
		assert(curr->part_context == NULL);
	}

	sl_thd_exit();
}

#endif /* PART_H */
