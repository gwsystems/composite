#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "rumpcalls.h"
#include "cos_init.h"

extern struct cos_compinfo booter_info;

void
hw_irq_alloc(void){

	int i, ret;
	/* timer interrupt handling is from VKERN */
	irq_thdcap[0] = irq_thdid[0] = irq_tcap[0] = irq_arcvcap[0] = 0;

	for(i = 1; i < HW_ISR_LINES; i++){
		irq_tcap[i] = cos_tcap_alloc(&booter_info, TCAP_PRIO_MAX);
		assert(irq_tcap[i]);
		irq_thdcap[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, cos_irqthd_handler, i);
		assert(irq_thdcap[i]);
		irq_thdid[i] = (thdid_t)cos_introspect(&booter_info, irq_thdcap[i], 9);
		assert(irq_thdid[i]);
		
		irq_arcvcap[i] = cos_arcv_alloc(&booter_info, irq_thdcap[i], irq_tcap[i], booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
		assert(irq_arcvcap[i]);

		if ((ret = cos_tcap_transfer(irq_arcvcap[i], BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, TCAP_PRIO_MAX))) {
			printc("Irq %d Tcap transfer failed %d\n", i, ret);
			assert(0);
		}

		cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, 32 + i, irq_arcvcap[i]);
	}
}
void
rump_booter_init(void)
{
	extern int vmid;

	char *json_file = "";

	/* nginx */
/*	char *json_file = "";
	if(vmid == 0) {
		json_file = "{,\"blk\":{,\"source\":\"dev\",\"path\":\"/dev/paws\",\"fstype\":\"cd9660\",\"mountpoint\":\"data\",},\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"nginx.bin\",},\0";
	}
*/	
	/* nginx baretmetal*/
/*	char *json_file = "";
	if(vmid == 0) {
		json_file = "{,\"blk\":{,\"source\":\"dev\",\"path\":\"/dev/paws\",\"fstype\":\"cd9660\",\"mountpoint\":\"data\",},\"net\":{,\"if\":\"wm0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"192.168.0.2\",\"mask\":\"24\",},\"cmdline\":\"nginx.bin\",},\0";
	}
*/
	/*paws in qemu*/
/*	if(vmid == 0){
		json_file = "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"paws.bin\",},\0";
	}
*/
	

	/* paws baremetal */
	printc("~~~~~ vmid: %d ~~~~~\n");
	if(vmid == 0) {
		json_file = "{,\"net\":{,\"if\":\"wm0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"192.168.0.2\",\"mask\":\"24\",},\"cmdline\":\"paws.bin\",},\0";
	}


	printc("\nRumpKernel Boot Start.\n");
	cos2rump_setup();
	/* possibly pass in the name of the program here to see if that fixes the name bug */

	printc("\nSetting up arcv for hw irq\n");
	if(vmid == 0) hw_irq_alloc();
	//RK_hw_irq
	
	//bmk_isr_init(ipintr, NULL, 12);
	
	/* We pass in the json config string to the RK */
	cos_run(json_file);
	printc("\nRumpKernel Boot done.\n");

	cos_vm_exit();
	return;
}
