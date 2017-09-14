#include "micro_booter.h"
#include "vk_api.h"
#include "spinlib.h"

#include "timer_inv_api.h"

#define DL_SPIN_US (4*1000) //4ms
#define DL_LOG_SIZE 128
extern int vmid;
static u32_t dl_made, dl_missed, dl_total;
static cycles_t next_deadline;

void
log_info(char *data)
{
	PRINTC("%s", data);
	cos_asnd(APP_CAPTBL_SELF_IOSND_BASE, 0);
	printc("-");
}

void
dlapp_init(void *d)
{
	char log[DL_LOG_SIZE] = { '\0' };

	printc("DL APP STARTED!\n");
	while (1) {
		cycles_t now;
		//u32_t rcvd_total = 0;

		timer_upcounter_wait(dl_total);

		if (!next_deadline) {
			next_deadline  = hpet_first_period() + (cycs_per_usec * HPET_PERIOD_US);
			/* ignoring first period */
			next_deadline += (cycs_per_usec * HPET_PERIOD_US);
		}

		//assert(dl_total + 1 == rcvd_total);

		spinlib_usecs(DL_SPIN_US);

		rdtscll(now);
		dl_total ++;

		if (now > next_deadline) dl_missed ++;
		else                dl_made ++;

		if ((dl_total % 1000) == 0) {
			memset(log, 0, DL_LOG_SIZE);
			sprintf(log, "Deadlines T:%u, =:%u, x:%u\n", dl_total, dl_made, dl_missed);
			log_info(log);
		}

		next_deadline += (cycs_per_usec * HPET_PERIOD_US);
	}
}
