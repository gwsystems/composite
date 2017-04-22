#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "rumpcalls.h"
#include "cos_init.h"
#include "micro_booter.h"

extern struct cos_compinfo booter_info;
extern int vmid;
extern unsigned int cycs_per_usec;

int dls_missed = 0;
int dls_made = 0;
int periods = 0;

static inline void spin_usecs_iters(microsec_t usecs) __attribute__((optimize("O0")));
static inline void spin_usecs(microsec_t usecs) __attribute__((optimize("O0")));
static inline int spin_usecs_dl(microsec_t usecs, cycles_t dl) __attribute__((optimize("O0")));
void dl_work_one(void *) __attribute__((optimize("O0")));
void dl_work_two(void *) __attribute__((optimize("O0")));
void test_deadline(thdcap_t, thdcap_t) __attribute__((optimize("O0")));

static inline void
spin_usecs(microsec_t usecs)
{
	cycles_t cycs = cycs_per_usec * usecs;
	cycles_t now;
	cycles_t now2;


	rdtscll(now);
	now2 = now;

	cycs += now;
	
	while (now < cycs) {
		rdtscll(now);
		assert(now > now2);
	}
//	if(periods == 2000) printc("c:%llu n1:%llu n2:%llu \n", cycs, now, now2, (now2 - cycs)/cycs_per_usec);
}

/*
 * returns 1 if deadline missed.
 *         0 if made.
 */
static inline int
spin_usecs_dl(microsec_t usecs, cycles_t dl)
{
	cycles_t now;

	spin_usecs(usecs);
	rdtscll(now);

	if (now > dl) return 1;
	
	return 0;
}

void
dl_work_two(void * ignore)
{
	while(1) {
		spin_usecs(2200);
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
}

void
dl_work_one(void * ignore)
{
	while(1) {
		spin_usecs(2800);
		cos_thd_switch( *((thdcap_t *)ignore) );
	}
}

cycles_t last;
cycles_t first = 0;
cycles_t deadline = 0;
cycles_t prev_exec = 0;

void
test_deadline(thdcap_t dl_wrk_thd1, thdcap_t dl_wrk_thd2) {
	cycles_t now;
	cycles_t then;

	rdtscll(then);
	cos_thd_switch(dl_wrk_thd1);
	rdtscll(now);
	prev_exec = now;
	
	if (now <= deadline || cycles_same(now, deadline, 1<<10)) dls_made ++;
	else							  dls_missed ++;
}

void 
dl_booter_init(void)
{
	cycles_t first_period = 0, first_start, first_dl;
	tcap_res_t budget = 0;
	cycles_t activation = 0;
	int ret = 0;
	thdcap_t dl_wrk_thd1, dl_wrk_thd2;

	printc("DL_BOOTER_INIT: %d\n", vmid);
	assert(cycs_per_usec && cycs_per_msec);

	dl_wrk_thd1 = cos_thd_alloc(&booter_info, booter_info.comp_cap, dl_work_one, (thdcap_t *) &dl_wrk_thd2);
	assert(dl_wrk_thd1);
	
	dl_wrk_thd2 = cos_thd_alloc(&booter_info, booter_info.comp_cap, dl_work_two, NULL);
	assert(dl_wrk_thd2);
	
	while(1) {
		cycles_t now;

		ret = cos_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE);
//		if (!TCAP_RES_IS_INF(budget))
//			budget = (tcap_res_t)cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_GET_BUDGET); 
//		rdtscll(now);
//		if (activation == 0) activation = now;
//		else {
//			if (dls_missed && dls_missed < 10)
//				printc("p%d act:%llu, now:%llu, pdl:%llu:%llu, budget:%lu\n", periods, now - activation, now, deadline, prev_exec, budget);
//			activation = now;
//			if (dls_missed == 10) while (1) ;
//		}

		if (deadline == 0) {
			rdtscll(first_start);
			deadline = hpet_first_period() + (HPET_PERIOD_MS*cycs_per_msec);
			first_dl = deadline;
			first_period = hpet_first_period();
			
		} else {
			deadline = deadline + (HPET_PERIOD_MS*cycs_per_msec);
		}

		test_deadline(dl_wrk_thd1, dl_wrk_thd2);	
		
		periods++;
		if (periods % 1000 == 0) printc("periods:%d, dl_missed:%d, dl_made:%d\n", periods, dls_missed, dls_made);
	}
}

