#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sl_thd.h>
#include <llprint.h>

#include "micro_booter.h"
#include "cos2rk_types.h"
#include "timer_inv_api.h"

/*
 * TODO:
 *	1. A thread for attaching with HPET interrupt, AEP.
 *	2. A thread for LA app to query HPET diag (number of hpets received), AEP with it's own TCAP but uses LA passed budget! + this thread is actually created by vkernel when setting up LA app!
 *	3. A sinv for HA app to query number of HPETs received!
 *      4. Can implement all of this with SL? Lets see!
 */
#define HA_COMP_NUM_THDS 3
#define HA_HPET_THD_PRIO 1
#define HA_APP_THD_PRIO 2
#define HA_IO_THD_PRIO 3 /* i/o thread used for LA request */

struct sl_thd *local_thds[HA_COMP_NUM_THDS];
void hpet_handler(arcvcap_t rcv, void *data);

static void
__timersub_thds_init(void)
{
	struct cos_defcompinfo child_defcinfo;
	struct cos_compinfo *child_ci = cos_compinfo_get(&child_defcinfo);
	struct cos_aep_info *child_aep = cos_sched_aep_get(&child_defcinfo);
	union sched_param_union spprio = {.c = {.type = SCHEDP_PRIO, .value = HA_HPET_THD_PRIO}};

	/* create hpet periodic timer thread */
	local_thds[0] = sl_thd_aep_alloc(hpet_handler, NULL, 0);
	assert(local_thds[0]);
	sl_thd_param_set(local_thds[0], spprio.v);

	/* just set what's required for sl! */
	child_ci->captbl_cap = BOOT_CAPTBL_SELF_CT;
	child_aep->thd       = VM_CAPTBL_SELF_APPTHD_BASE;	
	child_aep->rcv       = BOOT_CAPTBL_SELF_INITRCV_BASE;
	child_aep->tc        = BOOT_CAPTBL_SELF_INITTCAP_BASE; 

	/* init the app thread */
	local_thds[1]  = sl_thd_comp_init(&child_defcinfo, 0);
	assert(local_thds[1]);
	spprio.c.value = HA_APP_THD_PRIO;
	sl_thd_param_set(local_thds[1], spprio.v);
	
	/* init the i/o thread */
	child_aep->thd = SUB_CAPTBL_SELF_IOTHD_BASE;
	child_aep->rcv = SUB_CAPTBL_SELF_IORCV_BASE;
	child_aep->tc  = TM_CAPTBL_SELF_IOTCAP_BASE;
	local_thds[2] = sl_thd_init(child_aep, 1);
	assert(local_thds[2]);
	spprio.c.value = HA_IO_THD_PRIO;
	sl_thd_param_set(local_thds[2], spprio.v);
//	spprio.c.type  = SCHEDP_PERIOD;
//	spprio.c.value = HPET_PERIOD_US; /* mainly used for blocking for period if no budget! performance! */
//	sl_thd_param_set(local_thds[2], spprio.v);

	/* attach to hpet periodic timer */
	cos_hw_periodic_attach(BOOT_CAPTBL_SELF_INITHW_BASE, sl_thd_rcvcap(local_thds[0]), HPET_PERIOD_US);
	//hpet_first_period();
	printc("Done timer sub init..\n");
}

void
timersub_init(void *d)
{
	printc("Timer Subsystem [%u] STARTED\n", cos_thdid());
	sl_init(CHILD_PERIOD_US);

	__timersub_thds_init();

	sl_sched_loop();
	printc("Timer Subsystem Scheduling Error!!\n");

	assert(0);
}
