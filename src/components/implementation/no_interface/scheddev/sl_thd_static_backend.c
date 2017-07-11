/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2017, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#include <sl.h>
#include <consts.h>
#include <ps.h>
#include <cos_kernel_api.h>

static struct sl_thd_policy sl_threads[SL_MAX_NUM_THDS];

/* Default implementations of backend functions */
struct sl_thd_policy *
sl_thd_alloc_backend(thdid_t tid)
{
	assert(tid < SL_MAX_NUM_THDS);
	return &sl_threads[tid];
}

void
sl_thd_free_backend(struct sl_thd_policy *t)
{ }

void
sl_thd_index_add_backend(struct sl_thd_policy *t)
{ }

void
sl_thd_index_rem_backend(struct sl_thd_policy *t)
{ }

struct sl_thd_policy *
sl_thd_lookup_backend(thdid_t tid)
{
	assert(tid < SL_MAX_NUM_THDS);
	return &sl_threads[tid];
}

void
sl_thd_init_backend(void)
{ assert(SL_MAX_NUM_THDS <= MAX_NUM_THREADS); }
