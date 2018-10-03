/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <sl.h>
#include <res_spec.h>
#include <hypercall.h>
#include <sched_info.h>
#include "test_vm.h"
#include <capmgr.h>

u32_t cycs_per_usec = 0;

#define FIXED_PRIO 1
#define FIXED_PERIOD_US (10000)
#define FIXED_BUDGET_US (10000)

void
sched_child_init(struct sched_childinfo *schedci)
{
	struct sl_thd *initthd = NULL;
	int ret;

	assert(schedci);
	initthd = sched_child_initthd_get(schedci);
	assert(initthd);

	if ((ret = cos_tcap_transfer(sl_thd_rcvcap(initthd), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX))) {
		PRINTC("%s: Failed to transfer INF budget\n");
		assert(0);
	}
	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));
	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_WINDOW, FIXED_PERIOD_US));
	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_BUDGET, FIXED_BUDGET_US));
}

#ifdef VM_IPC_TEST
void
__server_fn(arcvcap_t r, void *d)
{
	asndcap_t clientasnd = 0;

#ifdef TEST_IPC_RAW
	while (clientasnd == 0) clientasnd = capmgr_asnd_key_create_raw(CLIENT_XXX_AEPKEY);
#else
	while (clientasnd == 0) clientasnd = capmgr_asnd_key_create(CLIENT_XXX_AEPKEY);
#endif

	while (1) {
		int pending = 0, rcvd = 0, ret = 0;

		pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(pending == 0 && rcvd >= 1);

		ret = cos_asnd(clientasnd, 0);
		assert(ret == 0);
	}
}

void
test_aep_create(void)
{
	int ret;
	struct sl_thd *__srv_thd = NULL;

	if (cos_cpuid() != SERVER_CORE) return;

	__srv_thd = sl_thd_aep_alloc(__server_fn, NULL, 1, SERVER_XXX_AEPKEY, 0, 0);
	assert(__srv_thd);
	if ((ret = cos_tcap_transfer(sl_thd_rcvcap(__srv_thd), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX))) {
		PRINTC("%s: Failed to transfer INF budget\n");
		assert(0);
	}

	sl_thd_param_set(__srv_thd, sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));
	sl_thd_param_set(__srv_thd, sched_param_pack(SCHEDP_WINDOW, FIXED_BUDGET_US));
	sl_thd_param_set(__srv_thd, sched_param_pack(SCHEDP_BUDGET, FIXED_PERIOD_US));
}
#endif

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	static volatile int first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };
	static u32_t cpubmp[NUM_CPU_BMP_WORDS] = { 0 };
	int i;

	PRINTLOG(PRINT_DEBUG, "CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	if (ps_cas((unsigned long *)&first, NUM_CPU + 1, cos_cpuid())) {
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();
		cos_init_args_cpubmp(cpubmp);
	} else {
		while (!ps_load((unsigned long *)&init_done[first])) ;

		cos_defcompinfo_sched_init();
	}
	ps_faa((unsigned long *)&init_done[cos_cpuid()], 1);

	/* make sure the INITTHD of the scheduler is created on all cores.. for cross-core sl initialization to work! */
	for (i = 0; i < NUM_CPU; i++) {
		if (!bitmap_check(cpubmp, i)) continue;

		while (!ps_load((unsigned long *)&init_done[i])) ;
	}

	sl_init_cpubmp(SL_MIN_PERIOD_US * 100, cpubmp);
	sched_childinfo_init();

#ifdef VM_IPC_TEST
	test_aep_create();
#endif

	self_init[cos_cpuid()] = 1;
	hypercall_comp_init_done();

	sl_sched_loop_nonblock();

	PRINTLOG(PRINT_ERROR, "Should never have reached this point!!!\n");
	assert(0);
}
