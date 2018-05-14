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
#include <cos_defkernel_api.h>

static struct sl_thd_policy __sl_threads[NUM_CPU][SL_MAX_NUM_THDS];

static struct cos_aep_info __sl_aep_infos[NUM_CPU][SL_MAX_NUM_THDS];
static u32_t               __sl_aep_free_off[NUM_CPU];

/* Default implementations of backend functions */
struct sl_thd_policy *
sl_thd_alloc_backend(thdid_t tid)
{
	assert(tid < SL_MAX_NUM_THDS);

	return &(__sl_threads[cos_cpuid()][tid]);
}

struct cos_aep_info *
sl_thd_alloc_aep_backend(void)
{
	struct cos_aep_info *aep = NULL;

	assert(__sl_aep_free_off[cos_cpuid()] < SL_MAX_NUM_THDS);
	aep = &(__sl_aep_infos[cos_cpuid()][__sl_aep_free_off[cos_cpuid()]]);
	ps_faa((unsigned long *)&(__sl_aep_free_off[cos_cpuid()]), 1);

	return aep;
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

	return &(__sl_threads[cos_cpuid()][tid]);
}

void
sl_thd_init_backend(void)
{
	assert(SL_MAX_NUM_THDS <= MAX_NUM_THREADS);

	memset(__sl_threads[cos_cpuid()], 0, sizeof(struct sl_thd_policy)*SL_MAX_NUM_THDS);
	memset(__sl_aep_infos[cos_cpuid()], 0, sizeof(struct cos_aep_info)*SL_MAX_NUM_THDS);
	__sl_aep_free_off[cos_cpuid()] = 0;
}
