#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <ps.h>

#include "rumpcalls.h"
#include "cos_init.h"
#include "micro_booter.h"
#include "spin.h"

extern struct cos_compinfo booter_info;
extern int vmid;
extern unsigned int cycs_per_usec;

cycles_t first = 0;
cycles_t deadline = 0;
cycles_t prev_exec = 0;

int dls_missed = 0;
int dls_made = 0;
int periods = 0;

thdcap_t w1 = 0, w2 = 0;
unsigned long run = 0;

void dl_work_one(void *) __attribute__((optimize("O0")));
void dl_work_two(void *) __attribute__((optimize("O0")));
void dl_deadline_test(void) __attribute__((optimize("O0")));

#define WORKLOAD1 ((u64_t)2000)
#define WORKLOAD2 ((u64_t)2800)

void
dl_work_two(void * ignore)
{
	unsigned long c = 2, n = 0;

	while(1) {
		assert(run == 2);

		spin_usecs(WORKLOAD2);
		if (!ps_cas(&run, c, n)) assert(0);
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
}

void
dl_work_one(void * ignore)
{
	unsigned long c = 1, n = 2;

	while(1) {
		assert(run == 1);

		spin_usecs(WORKLOAD1);
		if (!ps_cas(&run, c, n)) assert(0);
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
}

void
test_deadline(void) {
	cycles_t now;
	cycles_t then;

	rdtscll(then);
	run = 1;
	while (run > 0) {
		int ret;
		thdcap_t t = (run == 1) ? w1 : (run == 2 ? w2 : 0);

		assert(run > 0);
		assert((t == w1 && run == 1) || (t == w2 && run == 2));
		cos_thd_switch(t);
	}
	run = -1;
	rdtscll(now);
	prev_exec = now;

	if (deadline == 0) deadline = hpet_first_period() + (HPET_PERIOD_MS*cycs_per_msec);
	else 		   deadline = deadline + (HPET_PERIOD_MS*cycs_per_msec);

	if (now <= deadline) dls_made ++;
	else                 dls_missed ++;
}

void 
dl_booter_init(void)
{
	cycles_t first_period = 0, first_start, first_dl;
	tcap_res_t budget = 0;
	cycles_t activation = 0;

	printc("DL_BOOTER_INIT: %d\n", vmid);
	assert(cycs_per_usec && cycs_per_msec);

	w1 = cos_thd_alloc(&booter_info, booter_info.comp_cap, dl_work_one, NULL);
	assert(w1);
	
	w2 = cos_thd_alloc(&booter_info, booter_info.comp_cap, dl_work_two, NULL);
	assert(w2);
	
	while(1) {
		cycles_t now;

		cos_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE);
//		if (!TCAP_RES_IS_INF(budget))
//			budget = (tcap_res_t)cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_GET_BUDGET); 
//		rdtscll(now);
//		if (activation == 0) activation = now;
//		else {
//			if ((periods < 10) || (periods > 4500))
//				printc("%d=now:%llu,last dl:%llu end:%llu\n", periods, now, deadline, prev_exec);
//			activation = now;
//			if (periods == 4510) while (1) ;
//		}

		test_deadline();	
		
		periods++;
		if (periods % 1000 == 0) printc("periods:%d, dl_missed:%d, dl_made:%d\n", periods, dls_missed, dls_made);
	}
}

