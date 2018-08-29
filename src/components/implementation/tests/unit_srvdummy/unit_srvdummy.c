/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <srv_dummy.h>
#include <sched.h>
#include <schedinit.h>
#include <hypercall.h>
#include "perfdata.h"
#include <capmgr.h>

static struct perfdata pd;

#define TEST_PRIO 1
#define TEST_N_THDS 2

#define TEST_N_ITERS 1000000

static void
__srv_dummy_test(void)
{
	int r1 = 0, r2 = 0, r3 = 0;
	int a = cos_thdid(), b = 10 * cos_thdid(), c = 20 * cos_thdid();

	r1 = srv_dummy_hello(a, b, c);
	assert(r1 == (a + b + c));
	r1 = srv_dummy_goodbye(&r2, &r3, a, b, c);
	assert(r1 = c && r2 == b && r3 == a);
	a++;
	b++;
	c++;
}

static void
print_ipi_info(void)
{
	int i = 0;

	printc("====================================\n");
	printc("IPI Info:\n");
	for (i = 0; i < NUM_CPU; i ++) {
		unsigned int k_snd = 0, k_rcv = 0, c_snd = 0, c_rcv = 0;

		capmgr_core_ipi_counters_get(i, 0, &k_snd, &k_rcv);
		capmgr_core_ipi_counters_get(i, 1, &c_snd, &c_rcv);

		printc("CPU%d = %lu sent / %lu rcvd (kernel: %lu/%lu)\n", i, c_snd, c_rcv, k_snd, k_rcv);
	}
	printc("====================================\n");
}

static void
__srv_dummy_perf_test(void)
{
	cycles_t st, en, diff;
	int iters = 0;

	perfdata_init(&pd, "RPC");
	while (iters < TEST_N_ITERS) {
		rdtscll(st);
		srv_dummy_hello(0, 0, 0);
		rdtscll(en);

		diff = en - st;

		iters++;
		perfdata_add(&pd, diff);
	}

	perfdata_calc(&pd);
	perfdata_print(&pd);
	print_ipi_info();
}

int tdone[TEST_N_THDS] = { 0 };
static void
__test_thd_fn(void *d)
{
	if ((int)d == 999) {
		__srv_dummy_perf_test();
	} else {
		 __srv_dummy_test();
		tdone[(int)d] = 1;
	}

	sched_thd_exit();
}

thdid_t test_tid[TEST_N_THDS];

static void
test_thds(void)
{
	int i;

	for (i = 0; i < TEST_N_THDS; i++) {
		test_tid[i] = sched_thd_create(__test_thd_fn, (void *)i);
		assert(test_tid[i]);
		sched_thd_param_set(test_tid[i], sched_param_pack(SCHEDP_PRIO, TEST_PRIO));
	}
}

#define TEST_UBENCH

void
cos_init(void)
{
	spdid_t child;
	comp_flag_t childflag;
	int i = 999;

	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

	schedinit_child();
#ifndef TEST_UBENCH
	test_thds();
	__srv_dummy_test();

	for (i = 0; i < TEST_N_THDS; i++) {
		while (!tdone[i]) ;
	}
	PRINTLOG(PRINT_DEBUG, "SUCCESS: Cross-component SINV to ASYNC INVOCATIONS done!\n");
#else
	__test_thd_fn((void *)i);
#endif
	sched_thd_exit();
}
