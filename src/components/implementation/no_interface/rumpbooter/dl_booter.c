#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "rumpcalls.h"
#include "cos_init.h"

extern struct cos_compinfo booter_info;
extern int vmid;
extern unsigned int cycs_per_usec;

int dls_missed = 0;
int dls_made = 0;

static inline void
spin_usecs(microsec_t usecs)
{
	cycles_t cycs = cycs_per_usec * usecs;
	cycles_t now;

	rdtscll(now);
	cycs += now;
	
	while (now < cycs) rdtscll(now);
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
		spin_usecs(300);
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
}

void
dl_work_one(void * ignore)
{
	while(1) {
		spin_usecs(300);
		cos_thd_switch( *((thdcap_t *)ignore) );
	}
}

cycles_t last;

int
test_deadline(thdcap_t dl_wrk_thd1, thdcap_t dl_wrk_thd2) {
	cycles_t now, deadline;
	rdtscll(now);

	/*800 cycles is about the cost of HPET->DLVM interrupt path*/
	deadline = (now-800) + (cycs_per_usec * 1000);
	
	cos_thd_switch(dl_wrk_thd1);

	rdtscll(now);

	if (now > deadline) {
	       	dls_missed++;
		//printc("gap: %lu\n", (now - deadline)/cycs_per_usec);
	} else { 
		dls_made++;
	}
}


void
check_delegate(void) {
		cycles_t now;
		rdtscll(now);
		
		//tcap_res_t min = VIO_BUDGET_APPROX * cycs_per_usec;
		tcap_res_t min = 1000 * 10;

		if ((last - now) > min) {

			rdtscll(last);
			tcap_res_t budget = (tcap_res_t)cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_GET_BUDGET);
			tcap_res_t res;
			
			if (budget >= min) res = 1000 * 10; 
			else res = 0; /* 0 = 100% budget */
			if(res == 0) printc("res = 0 %lu\n", budget);
			if(cos_tcap_delegate(VM_CAPTBL_SELF_IOASND_BASE, BOOT_CAPTBL_SELF_INITTCAP_BASE, res, VIO_PRIO, 0)) assert(0);
		}
}

void 
dl_booter_init(void)
{
	printc("DL_BOOTER_INIT: %d\n", vmid);
	thdcap_t dl_wrk_thd1, dl_wrk_thd2;
	thdid_t  dl_wrk_thdid, dl_wrk_thdid2;
	static int periods = 0;

	dl_wrk_thd1 = cos_thd_alloc(&booter_info, booter_info.comp_cap, dl_work_one, (thdcap_t *) &dl_wrk_thd2);
	assert(dl_wrk_thd1);
	dl_wrk_thdid = (thdid_t) cos_introspect(&booter_info, dl_wrk_thd1, THD_GET_TID);
	
	dl_wrk_thd2 = cos_thd_alloc(&booter_info, booter_info.comp_cap, dl_work_two, NULL);
	assert(dl_wrk_thd2);
	dl_wrk_thdid2 = (thdid_t) cos_introspect(&booter_info, dl_wrk_thd2, THD_GET_TID);

	printc("\tDL worker thread= cap:%d tid:%d\n", (unsigned int)dl_wrk_thd1, dl_wrk_thdid);
	rdtscll(last);
	
	//do delegate to hpet	
	if(cos_tcap_delegate(VM_CAPTBL_SELF_IOASND_BASE, BOOT_CAPTBL_SELF_INITTCAP_BASE, 10000, VIO_PRIO, 0)) assert(0);
	
	while(1) {
		cos_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE);
		//printc("w\n");
		test_deadline(dl_wrk_thd1, dl_wrk_thd2);	
		periods++;
		if (periods > 100000) printc("dl_missed: %d   dl_made: %d\n", dls_missed, dls_made);
		
		check_delegate();
	}
}

