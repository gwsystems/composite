/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
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
#define TEST_ITERS 1000
static struct cos_aep_info taeps[NUM_CPU][TEST_N_AEPS];
static asndcap_t __childasnd[NUM_CPU] = { 0 };

static u32_t cycs_per_usec = 0;
static int parent_sent[NUM_CPU], child_rcvd[NUM_CPU];

static void
__test_child(arcvcap_t rcv, void *data)
{
	int ret;

	assert(taeps[cos_cpuid()][(int)data].rcv == rcv);
	while (child_rcvd[cos_cpuid()] < TEST_ITERS) {
		ret = cos_rcv(rcv, 0, NULL);
		assert(ret >= 0);

		child_rcvd[cos_cpuid()]++;
	}
	/* do nothing */

	sched_thd_exit();
}

static void
__test_parent(arcvcap_t rcv, void *data)
{
	int ret;

	assert(taeps[cos_cpuid()][(int)data].rcv == rcv);
	while (parent_sent[cos_cpuid()] < TEST_ITERS) {
		ret = cos_rcv(rcv, 0, NULL);
		assert(ret >= 0);

		do {
			ret = cos_asnd(__childasnd[cos_cpuid()], 1);
		} while (ret == -EDQUOT); /* will trigger if rate exceeded */
		assert(ret == 0);

		parent_sent[cos_cpuid()]++;
	}

	sched_thd_exit();
}

#define PARENT_AEPKEY ((1<<4) | cos_cpuid())
#define CHILD_AEPKEY  ((2<<4) | cos_cpuid())

#define INIT_CPU 0
#define PARENT_CPU 0
#define CHILD_CPU 1
thdid_t tidp = 0, tidc = 0;

#define PARENT_AEPKEY_XCPU ((1<<4))
#define CHILD_AEPKEY_XCPU  ((2<<4))

#define IPIWIN 50000 /* 10 ms */
#define IPIMAX 80

static void
test_xcore_aeps(void)
{
	asndcap_t __parentasnd = 0;
	int ret;
	int i = 0;
	int iters = 0;

	if (cos_cpuid() == PARENT_CPU) {
		tidp = sched_aep_create(&taeps[cos_cpuid()][i], __test_parent, (void *)i, 0, PARENT_AEPKEY_XCPU, IPIWIN, IPIMAX);
		assert(tidp);
		while (__childasnd[cos_cpuid()] == 0) __childasnd[cos_cpuid()] = capmgr_asnd_key_create(CHILD_AEPKEY_XCPU);
		sched_thd_param_set(tidp, sched_param_pack(SCHEDP_PRIO, TEST_PRIO));
	}

	i ++;
	if (cos_cpuid() == CHILD_CPU) {
		tidc = sched_aep_create(&taeps[cos_cpuid()][i], __test_child, (void *)i, 0, CHILD_AEPKEY_XCPU, IPIWIN, IPIMAX);
		assert(tidc);
		while (!tidp) ;
		sched_thd_param_set(tidc, sched_param_pack(SCHEDP_PRIO, TEST_PRIO));
	}

	if (cos_cpuid() == INIT_CPU) {
		while (!__parentasnd) __parentasnd = capmgr_asnd_key_create(PARENT_AEPKEY_XCPU);

		while (iters < TEST_ITERS) {
			ret = cos_asnd(__parentasnd, 1);
			assert(ret == 0);

			iters ++;
		}
	}

	while (parent_sent[PARENT_CPU] != TEST_ITERS && child_rcvd[CHILD_CPU] != TEST_ITERS) ;
	if (INIT_CPU == cos_cpuid()) PRINTLOG(PRINT_DEBUG, "SUCCESS: sched component cross-core snd/rcv unit tests\n");
}

static void
test_aeps(void)
{
	thdid_t tidp, tidc;
	asndcap_t __parentasnd;
	int ret;
	int i = 0;

	tidp = sched_aep_create(&taeps[cos_cpuid()][i], __test_parent, (void *)i, 0, PARENT_AEPKEY, 0, 0);
	assert(tidp);

	i ++;
	tidc = sched_aep_create(&taeps[cos_cpuid()][i], __test_child, (void *)i, 0, CHILD_AEPKEY, 0, 0);
	assert(tidc);

	__childasnd[cos_cpuid()] = capmgr_asnd_create(cos_spd_id(), tidc);
	assert(__childasnd[cos_cpuid()]);
	__parentasnd = capmgr_asnd_key_create(PARENT_AEPKEY);
	assert(__parentasnd);

	sched_thd_param_set(tidp, sched_param_pack(SCHEDP_PRIO, TEST_PRIO));
	sched_thd_param_set(tidc, sched_param_pack(SCHEDP_PRIO, TEST_PRIO));

	ret = cos_asnd(__parentasnd, 1);
	assert(ret == 0);

	PRINTLOG(PRINT_DEBUG, "%s: sched component aep scheduling unit tests\n", (parent_sent[cos_cpuid()] == 0 || child_rcvd[cos_cpuid()] == 0) ? "FAILURE" : "SUCCESS");
}

void
cos_init(void)
{
	spdid_t child;
	comp_flag_t childflags;

	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflags) == -1);
	if (NUM_CPU > 1) test_xcore_aeps();
	else             test_aeps();

	sched_thd_exit();
}
