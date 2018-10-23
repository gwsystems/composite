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

#define SCHED_OVHD_TEST
#define WITH_HITHD

#undef NOSCHED_TEST

#define LO_PRIO 2
#define HI_PRIO 1

#undef  MC_YIELD_UBENCH_TEST

#define N_TESTTHDS 2

static volatile int testing = 1;
static struct perfdata pd;

/* TODO: find right poll periods.. 1ms, 10ms, 100ms */
#define HI_POLL_US_1 1000
#define HI_POLL_US_2 10000
#define HI_POLL_US_3 100000

#define HI_PERIOD_US HI_POLL_US_1

#define SPINITERS_10US (5850)
#define SPINITERS_TIMES (100*1000*10) //10s
#define LO_SPIN_ITERS 5

static void __spin_fn(void) __attribute__ ((optimize("O0")));

static inline void
__spin_fn(void)
{
	unsigned int spin;

	for (spin = 0; spin < SPINITERS_10US; spin++) {
		__asm__ __volatile__("nop": : :"memory");
	}
}

static cycles_t spin_cycs[LO_SPIN_ITERS] = { 0 };
static volatile unsigned int spin_iters = 0;

void
test_hi_fn(void *d)
{
#ifndef WITH_HITHD
	assert(0);
#endif
	while (likely(spin_iters < LO_SPIN_ITERS)) {
		//sl_thd_block_periodic(0);
		sl_thd_block_timeout(0, sl_now() + sl_usec2cyc(HI_PERIOD_US));
	}

	sl_thd_exit();
}

void
test_lo_fn(void *d)
{
	unsigned long i;

	while (likely(spin_iters < LO_SPIN_ITERS)) {
		cycles_t st, en;

		rdtscll(st);
		for (i = 0; i < SPINITERS_TIMES; i++) __spin_fn();
		rdtscll(en);
		spin_cycs[spin_iters] = en - st;
		spin_iters++;
	}

	for (i = 0; i < LO_SPIN_ITERS; i++) {
		printc("%llu\n", spin_cycs[i]);
	}
	printc("------------------------\n");

#ifndef NOSCHED_TEST
	sl_thd_exit();
#endif
}

void
test_sched_ovhd(void)
{
	int                     i;
	struct sl_thd          *hi, *lo;
	union sched_param_union hp = {.c = {.type = SCHEDP_PRIO, .value = HI_PRIO}};
	union sched_param_union lp = {.c = {.type = SCHEDP_PRIO, .value = LO_PRIO}};
	union sched_param_union hpp = {.c = {.type = SCHEDP_WINDOW, .value = HI_PERIOD_US}};

	assert(NUM_CPU == 1);

	PRINTC("Creating threads for sched overhead test\n");
#ifdef WITH_HITHD
	hi = sl_thd_alloc(test_hi_fn, NULL);
	assert(hi);
	//sl_thd_param_set(hi, hpp.v);
	sl_thd_param_set(hi, hp.v);
#endif

	lo = sl_thd_alloc(test_lo_fn, NULL);
	assert(lo);
	sl_thd_param_set(lo, lp.v);
	PRINTC("Done.\n");
}

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
	struct sl_thd          *threads[N_TESTTHDS];
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
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);

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
#ifdef MC_YIELD_UBENCH_TEST
	test_yields();
#endif
#ifndef NOSCHED_TEST
#ifdef SCHED_OVHD_TEST
	test_sched_ovhd();
#endif
	sl_sched_loop_nonblock();
#else
	test_lo_fn(NULL);
	SPIN();
#endif

	assert(0);

	return;
}
