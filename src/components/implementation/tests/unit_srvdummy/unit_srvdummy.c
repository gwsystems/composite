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

#define TEST_PRIO 1
#define TEST_N_THDS 2

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

int tdone[TEST_N_THDS] = { 0 };
static void
__test_thd_fn(void *d)
{
	__srv_dummy_test();
	tdone[(int)d] = 1;
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

void
cos_init(void)
{
	spdid_t child;
	comp_flag_t childflag;
	int i;

	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

	schedinit_child();
	test_thds();
	__srv_dummy_test();

	for (i = 0; i < TEST_N_THDS; i++) {
		while (!tdone[i]) ;
	}
	PRINTLOG(PRINT_DEBUG, "SUCCESS: Cross-component SINV to ASYNC INVOCATIONS done!\n");

	sched_thd_exit();
}
