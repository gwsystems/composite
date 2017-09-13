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
#define HA_COMP_NUM_THDS 3
#define HA_HPET_THD_PRIO 1
#define HA_APP_THD_PRIO 2
#define HA_SERV_THD_PRIO 3 /* server thread used for LA request */

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
		int rcvd = 0, pending = 0;

		pending = cos_rcv(rcv, 0, &rcvd);
		assert(pending == 0); /* if there are more pending, that means, we missed as many deadlines. */

		__hpet_counter ++;
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
	struct cos_defcompinfo child_defcinfo;
	struct cos_compinfo *child_ci = cos_compinfo_get(&child_defcinfo);
	struct cos_aep_info *child_aep = cos_sched_aep_get(&child_defcinfo);
	union sched_param_union spprio = {.c = {.type = SCHEDP_PRIO, .value = HA_HPET_THD_PRIO}};

	/* create hpet periodic timer thread */
	local_thds[0] = sl_thd_aep_alloc(__hpet_handler, NULL, 0);
	assert(local_thds[0]);
	sl_thd_param_set(local_thds[0], spprio.v);

	/* just set what's required for sl! */
	child_ci->captbl_cap = BOOT_CAPTBL_SELF_CT;
	child_aep->thd       = VM_CAPTBL_SELF_APPTHD_BASE;	
	child_aep->rcv       = 0;
	child_aep->tc        = 0; 
	local_thds[1] = sl_thd_comp_init(&child_defcinfo, 0);
	assert(local_thds[1]);
	spprio.c.value = HA_APP_THD_PRIO;
	sl_thd_param_set(local_thds[1], spprio.v);
	
	/* TODO: init the server thread */

	/* attach to hpet periodic timer */
	cos_hw_periodic_attach(BOOT_CAPTBL_SELF_INITHW_BASE, sl_thd_rcvcap(local_thds[0]), HPET_PERIOD_US);
}

int
timer_inv_entry(int a, int b, int c, int d)
{
	int ret = 0;

	switch(a) {
	case TIMER_APP_BLOCK:
	{
		tcap_time_t timeout     = (tcap_time_t)b;
		cycles_t abs_timeout, now;

		rdtscll(now);
		abs_timeout = tcap_time2cyc(timeout, now);

		/* calling thread must be the main thread! */
		sl_thd_block_timeout(0, abs_timeout);

		break;
	}
	case TIMER_GET_COUNTER:
	{
		ret = __hpet_counter;
		break;
	}
	default: assert(0);
	}

	return ret;
}

void
timer_comp_init(void *d)
{
	sl_init();

	__thds_init();

	sl_sched_loop();

	assert(0);
}
