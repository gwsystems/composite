#include "micro_booter.h"
#include "vk_api.h"
#include "spinlib.h"
/* get from ha_comp */

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

		do {
			//ha_comp_call();
	
			if (rcvd_total == dl_total) {
				cycles_t next;
				tcap_time_t timeout;
	
				next = first_hpet + (dl_total) * HPET_PERIOD_US * cycs_per_usec;
				timeout = tcap_cyc2time(next);
				vk_vm_block(timeout); /* for timeout = hpet period */
			}
		} while (rcvd_total == dl_total);

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
