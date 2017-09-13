#include "micro_booter.h"
#include "vk_api.h"
#include "spinlib.h"

#include "timer_inv_api.h"

#define DL_SPIN_US (2*1000) //2ms
#define DL_LOG_SIZE 128
extern int vmid;
static u32_t dl_made, dl_missed, dl_total;

void
la_comp_call(char *data)
{
	PRINTC("%s", data);
}

void
dlapp_init(void *d)
{
	char log[DL_LOG_SIZE] = { '\0' };
	cycles_t first_hpet = hpet_first_period();
	cycles_t deadline = first_hpet + cycs_per_usec * HPET_PERIOD_US;

	while (1) {
		cycles_t now;
		u32_t rcvd_total = 0;

		rcvd_total = timer_upcounter_wait(dl_total);

		assert(dl_total + 1 == rcvd_total);

		spinlib_usecs(DL_SPIN_US);

		rdtscll(now);
		dl_total ++;

		if (now > deadline) dl_missed ++;
		else                dl_made ++;

		memset(log, 0, DL_LOG_SIZE);
		sprintf(log, "DLs T:%u, =:%u, x:%u\n", dl_total, dl_made, dl_missed);
		la_comp_call(log);

		deadline += (cycs_per_usec * HPET_PERIOD_US);
	}
}
