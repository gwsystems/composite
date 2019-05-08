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
#include <part.h>

#define COS_GOMP_MAX_EXPLICIT_TASKS 1024
#define COS_GOMP_MAX_IMPLICIT_TASKS 512

static struct part_task _itasks[COS_GOMP_MAX_IMPLICIT_TASKS], _etasks[COS_GOMP_MAX_EXPLICIT_TASKS];
static unsigned _itask_free, _etask_free;

static inline struct part_task *
_cos_gomp_alloc_implicit(void)
{
	unsigned i = ps_faa((unsigned long *)&_itask_free, 1);

	assert(i < COS_GOMP_MAX_IMPLICIT_TASKS);
	return &_itasks[i];
}

static inline struct part_task *
_cos_gomp_alloc_explicit(void)
{
	unsigned i = ps_faa((unsigned long *)&_etask_free, 1);

	assert(i < COS_GOMP_MAX_EXPLICIT_TASKS);
	return &_etasks[i];
}

void
cos_gomp_init(void)
{
	memset(_itasks, 0, sizeof(struct part_task) * COS_GOMP_MAX_IMPLICIT_TASKS);
	memset(_etasks, 0, sizeof(struct part_task) * COS_GOMP_MAX_EXPLICIT_TASKS);
	_itask_free = _etask_free = 0;

	cos_omp_init();
	part_init();
}

static inline void
_gomp_parallel_start(struct part_task *pt, void (*fn) (void *), void *data, unsigned num_threads, unsigned flags)
{
	int parent_off;
	struct sl_thd *t = sl_thd_curr();
	struct part_task *parent = (struct part_task *)t->part_context;

	num_threads = num_threads ? ((num_threads > COS_GOMP_MAX_THDS) ? COS_GOMP_MAX_THDS : num_threads) : PART_MAX;
	part_task_init(pt, PART_TASK_T_WORKSHARE, parent, num_threads, fn, data);
	if (parent) {
		parent_off = part_task_add_child(parent, pt);
		assert(parent_off >= 0);
	}
	t->part_context = pt;

	if (num_threads > 1) part_list_append(pt);
}

static inline void
_gomp_parallel_end(struct part_task *pt)
{
	struct sl_thd *t = sl_thd_curr();

	/* implicit barrier */
	part_task_barrier(pt);

	if (pt->nthds > 1) part_list_remove(pt);

	t->part_context = pt->parent;
	part_task_remove_child(pt->parent, pt);
}

/* GOMP_parallel prototype from libgomp within gcc */
void
GOMP_parallel (void (*fn) (void *), void *data, unsigned num_threads,
	       unsigned int flags)
{
	struct part_task pt;

	_gomp_parallel_start(&pt, fn, data, num_threads, flags);
	fn(data);
	_gomp_parallel_end(&pt);
}

bool
GOMP_single_start(void)
{
	struct part_task *t = (struct part_task *)sl_thd_curr()->part_context;
	int i;
	int coff = part_task_work_thd_num(t);
	unsigned b = 1 << coff;

	assert(coff >= 0 && coff < (int)t->nthds);
	for (i = t->ws_off[coff] + 1; i < PART_MAX_WORKSHARES; i++) {
		struct part_workshare *pw = &t->ws[i];
		unsigned c;

		if (ps_load((unsigned long *)&pw->type) == PART_WORKSHARE_NONE) {
			/* perhaps one of the threads just converted it to a single */
			if (!ps_cas((unsigned long *)&pw->type, PART_WORKSHARE_NONE, PART_WORKSHARE_SINGLE)) assert(pw->type == PART_WORKSHARE_SINGLE);
		}
		if (ps_load((unsigned long *)&pw->type) != PART_WORKSHARE_SINGLE) continue;

retry_bmp:
		c = ps_load((unsigned long *)&pw->worker_bmp);
		/* if already went through this, should not have called start! */
		assert(!(c & b));

		/* 
		 * this thd, add to worker bmp to indicate it reached the construct.
		 * if this is the first to reach, then return "true", else "false".
		 *
		 * if cas failed, try again as you have to indicate that this thd
		 * has done this construct!
		 */
		if (ps_cas((unsigned long *)&pw->worker_bmp, c, c | b)) {
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

	part_task_barrier(t);
}

static inline bool
_gomp_loop_dynamic_next(struct part_task *t, struct part_workshare *w, long *s, long *e)
{
	long cn, left, wrk = 0;

retry:
	cn = ps_load((unsigned long *)&w->next);
	left = w->end - cn;

	if (left == 0) return false;
	/* todo: incr <= 0 */
	assert(w->inc > 0);

	wrk = w->chunk_sz;
	if (left < wrk) wrk = left;
	if (!ps_cas((unsigned long *)&w->next, cn, cn + wrk)) goto retry;

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
	int coff = part_task_work_thd_num(t);
	unsigned b = 1 << coff;

	assert(coff >= 0 && coff < (int)t->nthds);
	for (i = t->ws_off[coff] + 1; i < PART_MAX_WORKSHARES; i++) {
		struct part_workshare *pw = &t->ws[i];
		unsigned c;

		if (ps_load((unsigned long *)&pw->type) == PART_WORKSHARE_NONE) {
			/* perhaps one of the threads just converted it to a loop */
			if (!ps_cas((unsigned long *)&pw->type, PART_WORKSHARE_NONE, PART_WORKSHARE_LOOP_DYNAMIC)) assert(pw->type == PART_WORKSHARE_LOOP_DYNAMIC);
		}

		if (ps_load((unsigned long *)&pw->type) != PART_WORKSHARE_LOOP_DYNAMIC) continue;

retry_bmp:
		c = ps_load((unsigned long *)&pw->worker_bmp);
		/* if already went through this, should not have called start! */
		assert(!(c & b));

		/* 
		 * this thd, add to worker bmp to indicate it reached the construct.
		 */
		if (ps_cas((unsigned long *)&pw->worker_bmp, c, c | b)) t->ws_off[coff] = i;
		else goto retry_bmp;

		/* all threads participating will initialize to the same values */
		if (!pw->end) {
			pw->chunk_sz = chunk_size;
			pw->inc = incr;
			pw->st = start;
			pw->end = end;
		}

		if (istart && iend) return _gomp_loop_dynamic_next(t, pw, istart, iend);
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
	struct part_task pt;
	bool ret;

	_gomp_parallel_start(&pt, fn, data, num_threads, flags);
	ret = GOMP_loop_dynamic_start(start, end, incr, chunk_size, NULL, NULL);
	assert(ret == true);

	fn(data);
	_gomp_parallel_end(&pt);
}

bool
GOMP_loop_dynamic_next (long *istart, long *iend)
{
	struct part_task *t = (struct part_task *)sl_thd_curr()->part_context;
	unsigned coff = part_task_work_thd_num(t);
	int woff = t->ws_off[coff];

	woff = woff < 0 ? 0 : woff;
	t->ws_off[coff] = woff;
	assert(t->ws[woff].type == PART_WORKSHARE_LOOP_DYNAMIC);

	return _gomp_loop_dynamic_next(t, &t->ws[woff], istart, iend);
}

void
GOMP_loop_end (void)
{
	struct part_task *t = (struct part_task *)sl_thd_curr()->part_context;
	unsigned coff = part_task_work_thd_num(t);
	int woff = t->ws_off[coff], c = 0;

	assert(t->ws[woff].type == PART_WORKSHARE_LOOP_DYNAMIC);

	part_task_barrier(t);

//	do {
//		c = ps_load((unsigned long *)&t->nwsdone);
//	} while (!ps_cas((unsigned long *)&t->nwsdone, c, c | (1 << woff)));
}

void
GOMP_loop_end_nowait (void)
{
	struct part_task *t = (struct part_task *)sl_thd_curr()->part_context;
	unsigned coff = part_task_work_thd_num(t);
	int woff = t->ws_off[coff], c = 0;

	assert(t->ws[woff].type == PART_WORKSHARE_LOOP_DYNAMIC);
//	do {
//		c = ps_load((unsigned long *)&t->nwsdone);
//	} while (!ps_cas((unsigned long *)&t->nwsdone, c, c | (1 << woff)));
}

void
GOMP_critical_start (void)
{
//	/* TODO: a multi-core lock! */
//	sl_lock_take(&_cos_gomp_lock);
}

void
GOMP_critical_end (void)
{
//	/* TODO: a multi-core lock! */
//	sl_lock_release(&_cos_gomp_lock);
}
