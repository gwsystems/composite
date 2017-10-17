#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sl_thd.h>

#include "vk_types.h"
#include "timer_inv_api.h"
#include "micro_booter.h"

extern unsigned int cycs_per_usec;

static volatile u32_t __hpet_counter = 0;
static volatile cycles_t __last_hpet = 0;
static volatile u32_t *__hpet_shm_addr = (u32_t *)APP_SUB_SHM_BASE;

int timer_get_counter(void)
{
	return cos_sinv(APP_CAPTBL_SELF_TM_SINV_BASE, TIMER_GET_COUNTER, 0, 0, 0);
}

int timer_upcounter_wait(u32_t curr_count)
{
	return cos_sinv(APP_CAPTBL_SELF_TM_SINV_BASE, TIMER_UPCOUNTER_WAIT, (int)curr_count, 0, 0);
}

int timer_app_block(tcap_time_t timeout)
{
	return cos_sinv(APP_CAPTBL_SELF_TM_SINV_BASE, TIMER_APP_BLOCK, (int)timeout, 0, 0);
}

void
hpet_handler(arcvcap_t rcv, void *data)
{
	int first = 1;

        while (1) {
                cos_rcv(rcv, 0, NULL);

		/* ignoring first period */
		if (first) {
			first = 0;
			continue;
		}

		rdtscll(__last_hpet);
		__hpet_counter ++;
        }
}

void
timer_io_fn(void *d)
{
	arcvcap_t rcv = SUB_CAPTBL_SELF_IORCV_BASE;

	while (1) {
		int rcvd;

		cos_rcv(rcv, RCV_ALL_PENDING, &rcvd);

		*__hpet_shm_addr = __hpet_counter;
	}
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
	case TIMER_UPCOUNTER_WAIT:
	{
		u32_t    app_counter = (u32_t)b;
		cycles_t abs_timeout, now;

		/* TODO: atomic __hpet_counter! */
		while (app_counter == __hpet_counter) {
			rdtscll(now);
			abs_timeout = __last_hpet + (HPET_PERIOD_US * cycs_per_usec);

			/* calling thread must be the main thread! */
			sl_thd_block_timeout(0, abs_timeout);
		}

		ret = __hpet_counter;
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

