#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "rumpcalls.h"
#include "cos_init.h"

extern struct cos_compinfo booter_info;
extern int vmid;

void
dl_work_two(void * ignore)
{
	while(1) {
		int i;
		for(i = 0; i < 10; i ++) {
		}
		printc("--2--");
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
}

void
dl_work_one(void * ignore)
{
	while(1) {
		int i;
		for(i = 0; i < 10; i ++) {
		}
		printc("wut: %d\n", *((int*)ignore));
		cos_thd_switch( *((thdcap_t *)ignore) );
	}
}

void 
dl_booter_init(void)
{
	printc("DL_BOOTER_INIT: %d\n", vmid);
	thdcap_t dl_wrk_thd1;
	thdid_t  dl_wrk_thdid;
	thdcap_t dl_wrk_thd2;
	thdid_t  dl_wrk_thdid2;

	dl_wrk_thd1 = cos_thd_alloc(&booter_info, booter_info.comp_cap, dl_work_one, (thdcap_t *) &dl_wrk_thd2);
	assert(dl_wrk_thd1);
	dl_wrk_thdid = (thdid_t) cos_introspect(&booter_info, dl_wrk_thd1, THD_GET_TID);
	
	dl_wrk_thd2 = cos_thd_alloc(&booter_info, booter_info.comp_cap, dl_work_two, NULL);
	assert(dl_wrk_thd2);
	dl_wrk_thdid2 = (thdid_t) cos_introspect(&booter_info, dl_wrk_thd2, THD_GET_TID);

	printc("\tDL worker thread= cap:%d tid:%d\n", (unsigned int)dl_wrk_thd1, dl_wrk_thdid);
	
	while(1) {
		cos_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE);
		cos_thd_switch(dl_wrk_thd1);
		
		printc("n we back\n");	
		if( cos_tcap_delegate(VM_CAPTBL_SELF_VKASND_BASE, BOOT_CAPTBL_SELF_INITTCAP_BASE, 0, PRIO_LOW, TCAP_DELEG_YIELD)) assert(0); 
		printc("this shouldn't print\n");	
	}
}

