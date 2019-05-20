/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 *
 *
 * NOTE: There is no header file for this library! 
 *	 This is a backend for GOMP API in GCC and 
 *	 replaces LIBGOMP for composite!
 */

#include <res_spec.h>
#include <sl.h>
#include <sl_thd.h>
#include <sl_lock.h> /* for now, single core lock! */
#include <cos_omp.h>

#include "cos_gomp.h"
#include <crt_lock.h>
#include <part.h>

static struct crt_lock _glock; /* global lock for critical sections */

static inline struct part_task *
_cos_gomp_alloc_explicit(void)
{
	return part_task_alloc(0);
}

void
cos_gomp_init(void)
{
	static int first_one = NUM_CPU, init_done = 0;

	if (ps_cas(&first_one, NUM_CPU, cos_cpuid())) {
		crt_lock_init(&_glock);
		cos_omp_init();
		init_done = 1;
	} else {
		while(!ps_load(&init_done)) ;
	}
	part_init();
}

static inline void
_gomp_parallel_start(struct part_task *pt, void (*fn) (void *), void *data, unsigned num_threads, unsigned flags)
{
	int parent_off;
	struct sl_thd *t = sl_thd_curr();
	struct part_task *parent = (struct part_task *)t->part_context;

	if (parent) assert(ps_load(&in_main_parallel));

	num_threads = (num_threads == 0 || num_threads > COS_GOMP_MAX_THDS) ? COS_GOMP_MAX_THDS : num_threads;

	/* nesting? */
#if !defined(PART_ENABLE_NESTED)
	if (unlikely(parent)) num_threads = 1;
#endif

	pt->state = PART_TASK_S_ALLOCATED;
	part_task_init(pt, PART_TASK_T_WORKSHARE, parent, num_threads, fn, data, NULL);
	assert(pt->nthds == num_threads);
	if (unlikely(parent)) {
		parent_off = part_task_add_child(parent, pt);
		assert(parent_off >= 0);
	}
	t->part_context = pt;
	/* should not append to workshare list if it's a task with nthds == 1 */
	part_list_append(pt);
}

static inline void
_gomp_parallel_end(struct part_task *pt)
{
	/* implicit hard barrier. only master thread to deinit task and all other threads just go back to pool */
	part_task_end(pt);
}

/* GOMP_parallel prototype from libgomp within gcc */
void
GOMP_parallel (void (*fn) (void *), void *data, unsigned num_threads,
	       unsigned int flags)
{
	struct part_task *prt = NULL;
	struct part_task pt;

#if defined(PART_ENABLE_NESTED)
	prt = &pt
#else
	struct sl_thd *t = sl_thd_curr();
	struct part_task *parent = (struct part_task *)t->part_context;

	/* child parallel will not be nested, will be run by this thread and also not added to the global list */
	if(parent) prt = &pt;
	else       prt = &main_task;
#endif

	_gomp_parallel_start(prt, fn, data, num_threads, flags);
	fn(data);
	_gomp_parallel_end(prt);
}

bool
GOMP_single_start(void)
{
	struct part_task *t = (struct part_task *)sl_thd_curr()->part_context;
	int i;
	int coff = part_task_work_thd_num(t, PART_CURR_THD);
	unsigned b = 1 << coff;

	assert(coff >= 0 && coff < (int)t->nthds);
	for (i = t->ws_off[coff] + 1; i < PART_MAX_WORKSHARES; i++) {
		struct part_workshare *pw = &t->ws[i];
		unsigned c;

		if (ps_load(&pw->type) == PART_WORKSHARE_NONE) {
			/* perhaps one of the threads just converted it to a single */
			if (!ps_cas(&pw->type, PART_WORKSHARE_NONE, PART_WORKSHARE_SINGLE)) assert(pw->type == PART_WORKSHARE_SINGLE);
		}
		if (ps_load(&pw->type) != PART_WORKSHARE_SINGLE) continue;

retry_bmp:
		c = ps_load(&pw->worker_bmp);
		/* if already went through this, should not have called start! */
		assert(!(c & b));

		/* 
		 * this thd, add to worker bmp to indicate it reached the construct.
		 * if this is the first to reach, then return "true", else "false".
		 *
		 * if cas failed, try again as you have to indicate that this thd
		 * has done this construct!
		 */
		if (ps_cas(&pw->worker_bmp, c, c | b)) {
			t->ws_off[coff] = i;

			return c ? false : true;
		}
		goto retry_bmp;
	}

	assert(0); /* exceed the number of workshares? */

	return false;
}

void
GOMP_barrier (void)
{
	struct part_task *t = (struct part_task *)sl_thd_curr()->part_context;

	part_task_barrier(t, 0);
}

static inline bool
_gomp_loop_dynamic_next(struct part_task *t, struct part_workshare *w, long *s, long *e)
{
	long cn, left, wrk = 0;

retry:
	cn = ps_load(&w->next);
	left = w->end - cn;

	if (unlikely(left == 0)) return false;
	/* todo: incr <= 0 */
	assert(w->inc > 0);

	wrk = w->chunk_sz;
	if (unlikely(left < wrk)) wrk = left;
	if (!ps_cas(&w->next, cn, cn + wrk)) goto retry;

	*s = cn;
	*e = cn + wrk;

	return true;
}

bool
GOMP_loop_dynamic_start (long start, long end, long incr, long chunk_size,
			 long *istart, long *iend)
{
	struct part_task *t = (struct part_task *)sl_thd_curr()->part_context;
	int i;
	int coff = part_task_work_thd_num(t, PART_CURR_THD);
	unsigned b = 1 << coff;

	assert(coff >= 0 && coff < (int)t->nthds);
	for (i = t->ws_off[coff] + 1; i < PART_MAX_WORKSHARES; i++) {
		struct part_workshare *pw = &t->ws[i];
		unsigned c;

		if (ps_load(&pw->type) == PART_WORKSHARE_NONE) {
			/* perhaps one of the threads just converted it to a loop */
			if (!ps_cas(&pw->type, PART_WORKSHARE_NONE, PART_WORKSHARE_LOOP_DYNAMIC)) assert(pw->type == PART_WORKSHARE_LOOP_DYNAMIC);
		}

		if (ps_load(&pw->type) != PART_WORKSHARE_LOOP_DYNAMIC) continue;

retry_bmp:
		c = ps_load(&pw->worker_bmp);
		/* if already went through this, should not have called start! */
		assert(!(c & b));

		/* 
		 * this thd, add to worker bmp to indicate it reached the construct.
		 */
		if (ps_cas(&pw->worker_bmp, c, c | b)) t->ws_off[coff] = i;
		else goto retry_bmp;

		/* all threads participating will initialize to the same values */
		if (unlikely(!pw->end)) {
			pw->chunk_sz = chunk_size;
			pw->inc = incr;
			pw->st = start;
			pw->end = end;
		}

		if (likely(istart && iend)) return _gomp_loop_dynamic_next(t, pw, istart, iend);
		else return true;
	}

	assert(0);

	return false;
}

void
GOMP_parallel_loop_dynamic (void (*fn) (void *), void *data,
			    unsigned num_threads, long start, long end,
			    long incr, long chunk_size, unsigned flags)
{
	struct part_task *prt = NULL;
	struct part_task pt;
	bool ret;

#if defined(PART_ENABLE_NESTED)
	prt = &pt
#else
	struct sl_thd *t = sl_thd_curr();
	struct part_task *parent = (struct part_task *)t->part_context;

	/* child parallel will not be nested, will be run by this thread and also not added to the global list */
	if (parent) prt = &pt;
	else        prt = &main_task;
#endif

	_gomp_parallel_start(prt, fn, data, num_threads, flags);
	ret = GOMP_loop_dynamic_start(start, end, incr, chunk_size, NULL, NULL);
	assert(ret == true);

	fn(data);
	_gomp_parallel_end(prt);
}

bool
GOMP_loop_dynamic_next (long *istart, long *iend)
{
	struct part_task *t = (struct part_task *)sl_thd_curr()->part_context;
	unsigned coff = part_task_work_thd_num(t, PART_CURR_THD);
	int woff = t->ws_off[coff];

	if (unlikely(woff < 0)) t->ws_off[coff] = woff = 0;
	assert(t->ws[woff].type == PART_WORKSHARE_LOOP_DYNAMIC);

	return _gomp_loop_dynamic_next(t, &t->ws[woff], istart, iend);
}

void
GOMP_loop_end (void)
{
	struct part_task *t = (struct part_task *)sl_thd_curr()->part_context;
	unsigned coff = part_task_work_thd_num(t, PART_CURR_THD);
	int woff = t->ws_off[coff], c = 0;

	assert(t->ws[woff].type == PART_WORKSHARE_LOOP_DYNAMIC);

	part_task_barrier(t, 0);
}

void
GOMP_loop_end_nowait (void)
{
	struct part_task *t = (struct part_task *)sl_thd_curr()->part_context;
	unsigned coff = part_task_work_thd_num(t, PART_CURR_THD);
	int woff = t->ws_off[coff], c = 0;

	assert(t->ws[woff].type == PART_WORKSHARE_LOOP_DYNAMIC);
}

void
GOMP_critical_start (void)
{
	crt_lock_take(&_glock);
}

void
GOMP_critical_end (void)
{
	crt_lock_release(&_glock);
}

void
GOMP_task (void (*fn) (void *), void *data, void (*cpyfn) (void *, void *),
           long arg_size, long arg_align, bool if_clause, unsigned flags,
           void **depend, int priority)
{
	struct part_task *parent = (struct part_task *)sl_thd_curr()->part_context;
	int parent_off = -1, ret = -1;

	/* 
	 * There should be nothing that prevents us to enqueue a task that 
	 * has a dependency, in or out!
	 * The thread that pops this task should potentially do the dependency
	 * tracking before/after execution of the function.
	 */
	/* TODO: depend, flags, etc! */
	assert(depend == NULL);

	if (if_clause) {
		struct part_task *pt = _cos_gomp_alloc_explicit();
		struct part_data *d = part_data_alloc();
		char *arg = NULL;

		assert(pt && d);
		assert(arg_size + arg_align - 1 <= PART_MAX_DATA);
		memset(d->data, 0, PART_MAX_DATA);
		arg = (char *) (((uintptr_t) d->data + arg_align - 1)
                                & ~(uintptr_t) (arg_align - 1));
		if (cpyfn) cpyfn(arg, data);
		else       memcpy(arg, data, arg_size);

		assert(parent);
		part_task_init(pt, PART_TASK_T_TASK, parent, 1, fn, arg, d);
		parent_off = part_task_add_child(parent, pt);
		assert(parent_off >= 0);
		assert(pt->type == PART_TASK_T_TASK);

		do {
			ret = part_deque_push(pt);
		} while (ret == -EAGAIN);
		assert(ret == 0);
		/* wake up a thread that might potentially run this workload */
		part_pool_wakeup();
	} else {
		/* if_clause is false, task is an included/undeferred task */
		struct part_task pt;

		assert(parent);
		part_task_init(&pt, PART_TASK_T_TASK, parent, 1, fn, data, NULL);
		parent_off = part_task_add_child(parent, &pt);
		assert(parent_off >= 0);
		sl_thd_curr()->part_context = &pt;
		pt.workers[0] = PART_CURR_THD;

		if (cpyfn) {
			char buf[arg_size + arg_align - 1];
			char *arg = (char *) (((uintptr_t) buf + arg_align - 1)
					& ~(uintptr_t) (arg_align - 1));

			cpyfn(arg, data);
			fn(arg);
		} else {
			fn(data);
		}

		part_task_end(&pt);
		sl_thd_curr()->part_context = pt.parent;
	}
}

void
GOMP_taskwait (void)
{
	struct part_task *t = sl_thd_curr()->part_context;

	part_task_wait_children(t);
	/* no barriers of course! */
}
