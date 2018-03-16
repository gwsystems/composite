/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <res_spec.h>
#include <hypercall.h>
#include <sched.h>
#include <capmgr.h>

#define TEST_PRIO 1
#define TEST_N_AEPS 2
static struct cos_aep_info taeps[TEST_N_AEPS];
static asndcap_t __childasnd;

static u32_t cycs_per_usec = 0;
static int parent_sent = 0, child_rcvd = 0;

static void
__test_child(arcvcap_t rcv, void *data)
{
	int ret;

	assert(taeps[(int)data].rcv == rcv);
	ret = cos_rcv(rcv, 0, NULL);
	assert(ret == 0);

	/* do nothing */
	child_rcvd = 1;

	sched_thd_exit();
}

static void
__test_parent(arcvcap_t rcv, void *data)
{
	int ret;

	assert(taeps[(int)data].rcv == rcv);
	ret = cos_rcv(rcv, 0, NULL);
	assert(ret == 0);

	parent_sent = 1;
	ret = cos_asnd(__childasnd, 1);
	assert(ret == 0);

	sched_thd_exit();
}

#define PARENT_AEPKEY 1
#define CHILD_AEPKEY  2

static void
test_aeps(void)
{
	thdid_t tidp, tidc;
	asndcap_t __parentasnd;
	int ret;
	int i = 0;

	tidp = sched_aep_create(&taeps[i], __test_parent, (void *)i, 0, PARENT_AEPKEY);
	assert(tidp);

	i ++;
	tidc = sched_aep_create(&taeps[i], __test_child, (void *)i, 0, CHILD_AEPKEY);
	assert(tidc);

	__childasnd = capmgr_asnd_create(cos_spd_id(), tidc);
	assert(__childasnd);
	__parentasnd = capmgr_asnd_key_create(PARENT_AEPKEY);
	assert(__parentasnd);

	sched_thd_param_set(tidp, sched_param_pack(SCHEDP_PRIO, TEST_PRIO));
	sched_thd_param_set(tidc, sched_param_pack(SCHEDP_PRIO, TEST_PRIO));

	ret = cos_asnd(__parentasnd, 1);
	assert(ret == 0);

	PRINTC("%s: sched component aep scheduling unit tests\n", (parent_sent == 0 || child_rcvd == 0) ? "FAILURE" : "SUCCESS");
}

void
cos_init(void)
{
	spdid_t child;
	comp_flag_t childflags;

	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflags) == -1);
	test_aeps();

	sched_thd_exit();
}
