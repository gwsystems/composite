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
static inline struct sl_thd_policy *
sl_thd_alloc_backend_core(cpuid_t core, thdid_t tid)
{
	assert(tid < SL_MAX_NUM_THDS && core >= 0 && core < NUM_CPU);

	return &(__sl_threads[core][tid]);
}

static inline struct cos_aep_info *
sl_thd_alloc_aep_backend_core(cpuid_t core)
{
	int off = 0;
	struct cos_aep_info *aep = NULL;

	assert(core < NUM_CPU && core >= 0);
	off = ps_faa((unsigned long *)&__sl_aep_free_off[core], 1);
	assert(off < SL_MAX_NUM_THDS);
	aep = &__sl_aep_infos[core][off];

	return aep;
}

struct sl_thd_policy *
sl_thd_migrate_backend(struct sl_thd_policy *t, cpuid_t core)
{
	assert(core != cos_cpuid() && core >= 0 && core < NUM_CPU);

	struct cos_aep_info *a = sl_thd_alloc_aep_backend_core(core);
	struct cos_aep_info *b = sl_thd_aepinfo(sl_mod_thd_get(t));
	struct sl_thd_policy *tc = sl_thd_alloc_backend_core(core, b->tid);
	struct sl_thd *x = sl_mod_thd_get(tc), *y = sl_mod_thd_get(t);

	memset(a, 0, sizeof(struct cos_aep_info));
	a->tid = b->tid;
	a->thd = b->thd;
	assert(b->rcv == 0 && b->tc == 0);
	memset(b, 0, sizeof(struct cos_aep_info));

	memcpy(tc, t, sizeof(struct sl_thd_policy));
	x->aepinfo = a;
	memset(t, 0, sizeof(struct sl_thd_policy));

	return tc;
}

struct sl_thd_policy *
sl_thd_alloc_backend(thdid_t tid)
{
	return sl_thd_alloc_backend_core(cos_cpuid(), tid);
}

struct cos_aep_info *
sl_thd_alloc_aep_backend(void)
{
	return sl_thd_alloc_aep_backend_core(cos_cpuid());
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
