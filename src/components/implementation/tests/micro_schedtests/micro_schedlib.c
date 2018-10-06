/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <sl.h>
#include "perfdata.h"

#define N_TESTTHDS 2

static volatile int testing = 1;
static struct perfdata pd;
static volatile cycles_t st = 0;

void
test_thd_fn(void *data)
{
	cycles_t *c = NULL;

	if (data != NULL) c = (cycles_t *)data;
	while (testing) {
		if (c) rdtscll(*c);
		sl_thd_yield(0);
	}

	sl_thd_exit();
}

#define NITERS 1000000

void
test_thd_c0fn(void *data)
{
	int iters = 0;

	while (iters < NITERS) {
		cycles_t en;

		rdtscll(en);
		st = en;
		sl_thd_yield(0);
		rdtscll(en);

		perfdata_add(&pd, en - st);
		iters++;
	}

	testing = 0;
	perfdata_calc(&pd);
	perfdata_print(&pd);

	sl_thd_exit();
}

void
test_yields(void)
{
	int                     i;
	struct sl_thd *         threads[N_TESTTHDS];
	union sched_param_union sp = {.c = {.type = SCHEDP_PRIO, .value = 1}};

	for (i = 0; i < N_TESTTHDS; i++) {

		if (cos_cpuid() == 0) {
			if (i == 0) threads[i] = sl_thd_alloc(test_thd_c0fn, NULL);
			else        threads[i] = sl_thd_alloc(test_thd_fn, (void *)&st);
		} else {
			threads[i] = sl_thd_alloc(test_thd_fn, NULL);
		}
		assert(threads[i]);
		sl_thd_param_set(threads[i], sp.v);
	}
}

void
cos_init(void)
{
	int i;
	static volatile unsigned long first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);

//	assert(NUM_CPU > 1);
	if (ps_cas(&first, NUM_CPU + 1, cos_cpuid())) {
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();
	} else {
		while (!ps_load(&init_done[first])) ;

		cos_defcompinfo_sched_init();
	}
	ps_faa(&init_done[cos_cpuid()], 1);
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load(&init_done[i])) ;
	}
	sl_init(SL_MIN_PERIOD_US);
	test_yields();
	sl_sched_loop_nonblock();

	assert(0);

	return;
}
