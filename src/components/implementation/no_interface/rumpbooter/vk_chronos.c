#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sl_thd.h>

#include "vk_types.h"
#include "vk_structs.h"

#define MIN_TIMESLICE_US   (1000) //2ms
#define SCHED_TIMESLICE_US (9000) //10ms

extern unsigned int cycs_per_usec;

void
chronos_sched_loop(void)
{
#if defined(CHRONOS_ENABLED)
	int id;

	while (1) {
		for (id = 0; id < APP_START_ID; id ++) {
			struct sl_thd *sub = (vmx_info[id].inithd);
			int ret;
			tcap_res_t budget;

			assert(id == RUMP_SUB || id == TIMER_SUB);
			budget = (id == RUMP_SUB ? SCHED_TIMESLICE_US : MIN_TIMESLICE_US) * cycs_per_usec;

			ret = cos_tcap_delegate(sub->sndcap, BOOT_CAPTBL_SELF_INITTCAP_BASE, budget, id == RUMP_SUB ? RUMP_PRIO : TIMER_PRIO, TCAP_DELEG_YIELD);
			assert(ret == 0);
		}
	}
#else
	int ret;
	struct sl_thd *rksub = (vmx_info[RUMP_SUB].inithd);

	printc("Handing off to RK SUBSYSTEM Scheduler!\n");
	ret = cos_tcap_delegate(rksub->sndcap, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX, TCAP_DELEG_YIELD);
	assert(ret == 0);

	/*
	 * Ideally, with all the fixes, it should never get here.
	 * BUT, I see that even after disabling LAPIC, timer interrupts happen at times on QEMU.
	 * If this isn't a problem on HW, then we can take remove the following spin loop!
	 */
	while (1) {
		printc(".");
		cos_asnd(rksub->sndcap, 1);
	}

	printc("Error!! Should not get here!\n");
	assert(0);
#endif
}
