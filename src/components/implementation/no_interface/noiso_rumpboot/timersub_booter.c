#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sl_thd.h>
#include <llprint.h>

#include "micro_booter.h"
#include "cos2rk_types.h"
#include "timer_inv_api.h"
#include "rk_sched.h"

extern void hpet_handler(arcvcap_t rcv, void *data);
extern void dlapp_init(void *);

static void
__timersub_thds_init(void)
{
	struct sl_thd *intr, *dlapp;
        union sched_param_union spprio = {.c = {.type = SCHEDP_PRIO, .value = TIMER_PRIO}};

	dlapp = sl_thd_alloc(dlapp_init, NULL);
	assert(dlapp);
	sl_thd_param_set(dlapp, spprio.v);
	printc("%s:%u\n", __func__, dlapp->thdid);

	intr = sl_thd_aep_alloc(hpet_handler, NULL, 0);
	assert(intr);
	sl_thd_param_set(intr, spprio.v);
	printc("%s:%u\n", __func__, intr->thdid);

	/* attach to hpet periodic timer */
	cos_hw_periodic_attach(BOOT_CAPTBL_SELF_INITHW_BASE, sl_thd_rcvcap(intr), HPET_PERIOD_US);
	//hpet_first_period();
	printc("Done timer sub init..\n");
}

void
timersub_init(void *d)
{
	printc("Timer Subsystem [%u] STARTED\n", cos_thdid());

	__timersub_thds_init();

	return;
}
