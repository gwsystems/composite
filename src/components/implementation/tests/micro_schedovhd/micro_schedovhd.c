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
#include <sched.h>
#include <schedinit.h>
#include <cos_time.h>
#include "perfdata.h"

#define SCHED_OVHD_TEST
#define WITH_HITHD

#define LO_PRIO 2
#define HI_PRIO 1

static volatile int testing = 1;
static struct perfdata pd;

/* TODO: find right poll periods.. 1ms, 10ms, 100ms */
#define HI_POLL_US_1 1000
#define HI_POLL_US_2 10000
#define HI_POLL_US_3 100000

#define HI_PERIOD_US HI_POLL_US_1

#define SPINITERS_10US (5850)
#define SPINITERS_TIMES (100*1000*10) //10s
#define LO_SPIN_ITERS 10

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
	cycles_t st = time_now(), interval = time_usec2cyc(HI_PERIOD_US), abs_timeout = st + interval;

	while (likely(spin_iters < LO_SPIN_ITERS)) {
		cycles_t now;

		sched_thd_block_timeout(0, abs_timeout);

		//abs_timeout += interval;
		now = time_now();
		abs_timeout = now + interval - ((now - st) % interval);
	}

	sched_thd_exit();
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

	sched_thd_exit();
}

void
test_sched_ovhd(void)
{
	int                     i;
	thdid_t                 hi, lo;
	union sched_param_union hp = {.c = {.type = SCHEDP_PRIO, .value = HI_PRIO}};
	union sched_param_union lp = {.c = {.type = SCHEDP_PRIO, .value = LO_PRIO}};

	assert(NUM_CPU == 1);

	PRINTC("Creating threads for sched overhead test\n");
#ifdef WITH_HITHD
	hi = sched_thd_create(test_hi_fn, NULL);
	assert(hi);
	sched_thd_param_set(hi, hp.v);
#endif

	lo = sched_thd_create(test_lo_fn, NULL);
	assert(lo);
	sched_thd_param_set(lo, lp.v);
	PRINTC("Done.\n");
}

void
cos_init(void)
{
	int i;
	static volatile unsigned long first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };

	if (ps_cas(&first, NUM_CPU + 1, cos_cpuid())) {
	} else {
		while (!ps_load(&init_done[first])) ;
	}
	ps_faa(&init_done[cos_cpuid()], 1);
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load(&init_done[i])) ;
	}

	test_sched_ovhd();
	schedinit_child();

	sched_thd_exit();
	assert(0);
}
