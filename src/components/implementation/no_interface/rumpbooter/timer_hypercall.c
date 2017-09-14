#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sl_thd.h>

#include "vk_types.h"
#include "timer_inv_api.h"

extern unsigned int cycs_per_usec;

static volatile u32_t __hpet_counter = 0;
static volatile cycles_t __last_hpet = 0;

int timer_get_counter(void)
{
	return cos_sinv(VM_CAPTBL_SELF_TM_SINV_BASE, TIMER_GET_COUNTER, 0, 0, 0);
}

int timer_upcounter_wait(u32_t curr_count)
{
	return cos_sinv(VM_CAPTBL_SELF_TM_SINV_BASE, TIMER_UPCOUNTER_WAIT, (int)curr_count, 0, 0);
}

int timer_app_block(tcap_time_t timeout)
{
	return cos_sinv(VM_CAPTBL_SELF_TM_SINV_BASE, TIMER_APP_BLOCK, (int)timeout, 0, 0);
}

void
hpet_handler(arcvcap_t rcv, void *data)
{
	int first = 1;

        while (1) {
		int rcvd = 0, pending = 0;

                pending = cos_rcv(rcv, 0, &rcvd);
		//assert(pending == 0); /* if there are more pending, that means, we missed as many deadlines. */

		/* ignoring first period */
		if (first) {
			first = 0;
			continue;
		}

		rdtscll(__last_hpet);
		__hpet_counter ++;
//		printc("[%u %llu]", __hpet_counter, __last_hpet);
        }
}

//void
//timer_serv_thd_fn(void *data)
//{
//        /* TODO: vkernel cap offsets */
//        arcvcap_t rcv = 0;
//        asndcap_t snd = 0;
//
//        while (1) {
//                cos_rcv(rcv, 0, 0);
//
//                /* TODO: write to ring buffer and send */
//                cos_asnd(snd, 0);
//        }
//}

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

