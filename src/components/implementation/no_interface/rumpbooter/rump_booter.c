#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "rumpcalls.h"
#include "cos_init.h"

#define HW_ISR_LINES 32

extern struct cos_compinfo booter_info;

struct data {
	thdcap_t prev; // Thread to switch back to
	unsigned short int thdid;
};


static void
async_rk_fn(void)
{
	while(1) {
		printc("testing testin 123\n");
	}
}


void
rumptest_thd_fn(void *param)
{
	struct data *thd_meta = (struct data*)param;

	printc("In rumptest_thd_fn\n");
	printc("thdid: %d\n", thd_meta->thdid);

	printc("fetching thd id\n");
	thd_meta->thdid = cos_thdid();
	printc("thdid, is now: %d\n", thd_meta->thdid);

	printc("switching back to old thread\n");
	cos_thd_switch(thd_meta->prev);
	printc("Error: this should not print");
}

void
test_rumpthread(void)
{
	thdcap_t new_thdcap;
	thdcap_t current_thdcap;
	void *thd_meta;

	current_thdcap = BOOT_CAPTBL_SELF_INITTHD_BASE;

	struct data info;
	info.prev = current_thdcap;

	thd_meta = &info;
	//cos_thd_fn_t func_ptr = rumptest_thd_fn;

	new_thdcap = cos_thd_alloc(&booter_info, booter_info.comp_cap, rumptest_thd_fn, thd_meta);
	cos_thd_switch(new_thdcap);

	printc("switched back to old thread, thdid: %d\n", info.thdid);
}

void
hw_irq_alloc(void){

	int i;
	//cos_hw_detach(BOOT_CAPTBL_SELF_INITHW_BASE, HW_PERIODIC);

	for(i = 1; i < HW_ISR_LINES; i++){
		irq_thdcap[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, cos_irqthd_handler, i);
		irq_thdid[i] = (thdid_t)cos_introspect(&booter_info, irq_thdcap[i], 9);
		irq_tcap[i] = cos_tcap_split(&booter_info, BOOT_CAPTBL_SELF_INITTCAP_BASE, 0, 0);
		irq_arcvcap[i] = cos_arcv_alloc(&booter_info, irq_thdcap[i], irq_tcap[i], booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
		cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, 32 + i, irq_arcvcap[i]);
	}
}

void
rump_booter_init(void)
{
	printc("\n%s\n", __func__);
	thdcap_t tc;
	arcvcap_t rc;
	thdid_t tid;
	int rcving;
	cycles_t cycles;
	tcap_t tcc;


	//char *json_file = "{,\"blk\":{,\"source\":\"dev\",\"path\":\"/dev/paws\",\"fstype\":\"cd9660\",\"mountpoint\":\"data\",},\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"net\":{,\"if\":\"tun0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"111.111.111.0\",\"mask\":\"24\",\"gw\":\"111.111.111.0\",},\"cmdline\":\"nginx.bin\",},\0";
	char *json_file = "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"paws.bin\",},\0";
	//char *json_file = "";

	tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_rk_fn, NULL);
	assert(tc);
	tcc = cos_tcap_split(&booter_info, BOOT_CAPTBL_SELF_INITTCAP_BASE, 0, 0);
	rc = cos_arcv_alloc(&booter_info, tc, tcc, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);

	assert(rc);

	cos_rcv(rc, &tid, &rcving, &cycles);

	printc("\nRump Sched Test Start\n");
	test_rumpthread();
	printc("\nRump Sched Test End\n");

	printc("\nRumpKernel Boot Start.\n");
	cos2rump_setup();
	/* possibly pass in the name of the program here to see if that fixes the name bug */

	printc("\nSetting up arcv for hw irq\n");
	hw_irq_alloc();

	/* We pass in the json config string to the RK */
	printc("Json File:\n%s", json_file);
	cos_run(json_file);
	printc("\nRumpKernel Boot done.\n");

	BUG();
	return;
}
