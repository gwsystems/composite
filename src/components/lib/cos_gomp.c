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
#include <sl_lock.h> /* for now, single core lock! */
#include <cos_omp.h>

#define _THD_FIXED_PRIO 1
#define _THD_LOCAL_ACTIVATE(t) sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, _THD_FIXED_PRIO))
static struct sl_lock _cos_gomp_lock = SL_LOCK_STATIC_INIT();

static void
_cos_gomp_thd_fn(void *d)
{
	int *ndone = (int *)d;
	struct sl_thd *t = sl_thd_curr();
	struct cos_aep_info *a = sl_thd_aepinfo(t);
	cos_thd_fn_t fn = NULL;

	/*
	 * TODO:
	 * 1. Understand how gomp works with fn & data and what exactly is being passed!
	 * 2. If work-stealing.. well, where am I stealing from! (void *d) should help with that!
	 */

	assert(a->fn);
	fn = (cos_thd_fn_t)a->fn;
	fn(a->data);
	ps_faa((unsigned long *)ndone, 1);

	sl_thd_exit();
}

static inline unsigned
_cos_gomp_num_threads(unsigned num_thds)
{
	return num_thds > 0 ? num_thds : (unsigned)omp_get_max_threads();
}

/* GOMP_parallel prototype from libgomp within gcc */
void
GOMP_parallel (void (*fn) (void *), void *data, unsigned num_threads,
               unsigned int flags)
{
	/* FIXME: improve everything! */
	unsigned i;
	unsigned num_done = 0;

	num_threads = _cos_gomp_num_threads(num_threads);
	assert(num_threads <= MAX_NUM_THREADS);
	for (i = 1; i < num_threads; i++) {
		struct sl_thd *t = NULL;
		struct cos_aep_info *a = NULL;

		/* TODO: any handling of AEPs? */
		t = sl_thd_alloc(_cos_gomp_thd_fn, (void *)&num_done);
		assert(t);

		a       = sl_thd_aepinfo(t);
		a->fn   = (cos_aepthd_fn_t)fn;
		a->data = data;

		_THD_LOCAL_ACTIVATE(t);
	}

	sl_thd_yield(0);

	fn(data);
	ps_faa((unsigned long *)&num_done, 1);
	/* TODO: anything else to do in this master? thread */

	while (ps_load((unsigned long *)&num_done) < (unsigned long)num_threads) sl_thd_yield(0);
}

bool
GOMP_single_start (void)
{
	static thdid_t t = 0;

	/* TODO: intelligence! */
	if (ps_cas((unsigned long *)&t, 0, cos_thdid())) return true;
	if (t == cos_thdid()) return true;

	return false;
}

void
GOMP_barrier (void)
{
	/* TODO: intelligence to wait for all threads in the team! */ 
	sl_thd_yield(0);
}

void
GOMP_critical_start (void)
{
	/* TODO: a multi-core lock! */
	sl_lock_take(&_cos_gomp_lock);
}

void
GOMP_critical_end (void)
{
	/* TODO: a multi-core lock! */
	sl_lock_release(&_cos_gomp_lock);
}
