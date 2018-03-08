/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <res_spec.h>
#include <hypercall.h>
#include <schedmgr.h>
#include <capmgr.h>

#define TEST_PRIO 1
#define TEST_N_AEPS 2
static struct cos_aep_info taeps[TEST_N_AEPS];
static asndcap_t __childasnd;

static u32_t cycs_per_usec = 0;

static void
__test_child(arcvcap_t rcv, void *data)
{
	int ret;

	assert(taeps[(int)data].rcv == rcv);
	ret = cos_rcv(rcv, 0, NULL);
	assert(ret == 0);

	PRINTC("Child-aep received event.\n");
	/* do nothing */

	schedmgr_thd_exit();
}

static void
__test_parent(arcvcap_t rcv, void *data)
{
	int ret;

	assert(taeps[(int)data].rcv == rcv);
	ret = cos_rcv(rcv, 0, NULL);
	assert(ret == 0);
	PRINTC("Parent-aep received event.\n");

	ret = cos_asnd(__childasnd, 1);
	assert(ret == 0);

	schedmgr_thd_exit();
}

static void
test_aeps(void)
{
	thdid_t tidp, tidc;
	asndcap_t __parentasnd;
	int ret;
	int i = 0;

	PRINTC("Testing AEP creation/activation\n");

	tidp = schedmgr_aep_create(&taeps[i], __test_parent, (void *)i, 0);
	assert(tidp);

	i ++;
	tidc = schedmgr_aep_create(&taeps[i], __test_child, (void *)i, 0);
	assert(tidc);

	__childasnd = capmgr_asnd_create(cos_spd_id(), tidc);
	assert(__childasnd);
	__parentasnd = capmgr_asnd_create(cos_spd_id(), tidp);
	assert(__parentasnd);

	schedmgr_thd_param_set(tidp, sched_param_pack(SCHEDP_PRIO, TEST_PRIO));
	schedmgr_thd_param_set(tidc, sched_param_pack(SCHEDP_PRIO, TEST_PRIO));

	PRINTC("Sending event to parent-aep\n");
	ret = cos_asnd(__parentasnd, 1);
	assert(ret == 0);

	PRINTC("Done.\n");
}

void
cos_init(void)
{
	spdid_t child;
	comp_flag_t childflags;

	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflags) == -1);
	test_aeps();

	schedmgr_thd_exit();
}
