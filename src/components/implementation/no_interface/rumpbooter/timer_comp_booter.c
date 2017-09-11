#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sl_thd.h>

#include "cos2rk_types.h"

/*
 * TODO:
 *	1. A thread for attaching with HPET interrupt, AEP.
 *	2. A thread for LA app to query HPET diag (number of hpets received), AEP with it's own TCAP but uses LA passed budget! + this thread is actually created by vkernel when setting up LA app!
 *	3. A sinv for HA app to query number of HPETs received!
 *      4. Can implement all of this with SL? Lets see!
 */
#define HA_COMP_NUM_THDS 2
#define HA_HPET_THD_PRIO 1
#define HA_SERV_THD_PRIO 2 /* server thread used for LA request */

struct sl_thd *local_thds[HA_COMP_NUM_THDS];

static u32_t __hpet_counter = 0;

int
timer_comp_inv(int a, int b, int c)
{
	return __hpet_counter;
}

static void
__hpet_handler(arcvcap_t rcv, void *data)
{
	while (1) {
		int rcvd = 0;

		cos_rcv(rcv, RCV_ALL_PENDING, &rcvd);

		__hpet_counter += rcvd;
	}
}

void
timer_serv_thd_fn(void *data)
{
	/* TODO: vkernel cap offsets */
	arcvcap_t rcv = 0;
	asndcap_t snd = 0;

	while (1) {
		cos_rcv(rcv, 0, 0);

		/* TODO: write to ring buffer and send */
		cos_asnd(snd, 0);
	}
}

static void
__thds_init(void)
{
	union sched_param_union spprio = {.c = {.type = SCHEDP_PRIO, .value = HA_HPET_THD_PRIO}};

	/* create hpet periodic timer thread */
	local_thds[0] = sl_thd_aep_alloc(__hpet_handler, NULL, 0);
	assert(local_thds[0]);
	sl_thd_param_set(local_thds[0], spprio.v);

	/* TODO: init the server thread */

	/* attach to hpet periodic timer */
	cos_hw_periodic_attach(BOOT_CAPTBL_SELF_INITHW_BASE, sl_thd_rcvcap(local_thds[0]), HPET_PERIOD_US);
}

void
timer_comp_init(void *d)
{
	sl_init();

	__thds_init();

	sl_sched_loop();

	assert(0);
}
