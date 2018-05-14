/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <srv_dummy_types.h>
#include <sched.h>
#include <sinv_async.h>
#include <schedinit.h>
#include <hypercall.h>

#define TEST_PRIO 1
#define TEST_N_THDS 2

struct sinv_async_info sinv_info;

static void
__srv_dummy_test(void)
{
	int r1 = 0, r2 = 0, r3 = 0;
	int a = cos_thdid(), b = 10 * cos_thdid(), c = 20 * cos_thdid();

	r1 = acom_client_request(&sinv_info, SRV_DUMMY_HELLO, a, b, c, 0, 0);
	assert(r1 == (a + b + c));
}

int tdone[TEST_N_THDS] = { 0 };
static void
__test_thd_fn(arcvcap_t r, void *d)
{
	__srv_dummy_test();
	tdone[(int)d] = 1;
	sched_thd_exit();
}

struct cos_aep_info aeps[TEST_N_THDS];
thdid_t test_tid[TEST_N_THDS];

static void
test_thds(void)
{
	int i;

	for (i = 0; i < TEST_N_THDS; i++) {
		memset(&aeps[i], 0, sizeof(struct cos_aep_info));

		test_tid[i] = sched_aep_create(&aeps[i], __test_thd_fn, (void *)i, 0, SRV_DUMMY_RKEY(SRV_DUMMY_ISTATIC, i + 1), 0, 0);
		assert(test_tid[i]);
		acom_client_thread_init(&sinv_info, test_tid[i], aeps[i].rcv, SRV_DUMMY_RKEY(SRV_DUMMY_ISTATIC, i + 1), SRV_DUMMY_SKEY(SRV_DUMMY_ISTATIC, i + 1));

		sched_thd_param_set(test_tid[i], sched_param_pack(SCHEDP_PRIO, TEST_PRIO));
	}
}

void
cos_init(void)
{
	spdid_t child;
	comp_flag_t childflag;
	int i, ret;

	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

	acom_client_init(&sinv_info, SRV_DUMMY_INSTANCE(SRV_DUMMY_ISTATIC));

	schedinit_child();
	test_thds();

	/*
	 * NOTE: async communication is between 2 threads that are both AEPs, this is not..
	 * because it doesn't have a "rcv" endpoint attached to it.
	 */
	for (i = 0; i < TEST_N_THDS; i++) {
		while (!tdone[i]) ;
	}
	PRINTLOG(PRINT_DEBUG, "SUCCESS: Cross-component ASYNC communication test done!\n");

	sched_thd_exit();
}
