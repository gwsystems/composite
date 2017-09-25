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

int timer_get_counter(void)
{
	return (int)__hpet_counter;
}

int timer_upcounter_wait(u32_t curr_count)
{
	u32_t    app_counter = curr_count;
	cycles_t abs_timeout, now;

	/* TODO: atomic __hpet_counter! */
	while (app_counter == __hpet_counter) {
		rdtscll(now);
		abs_timeout = __last_hpet + (HPET_PERIOD_US * cycs_per_usec);

		/* calling thread must be the main thread! */
		sl_thd_block_timeout(0, abs_timeout);
	}

	return (int)__hpet_counter;
}

int timer_app_block(tcap_time_t timeout)
{
	assert(0);
	return 0;
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

