#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "rumpcalls.h"
#include "cos_init.h"
#include "vk_types_old.h"

extern struct cos_compinfo booter_info;
extern int vmid;

void
hw_irq_alloc(void){

	tcap_res_t budget;
	int i, ret;
	int first = 1, id = HW_ISR_FIRST;

	memset(irq_thdcap, 0, sizeof(irq_thdcap));
	memset(irq_thdid, 0, sizeof(irq_thdid));
	memset(irq_arcvcap, 0, sizeof(irq_arcvcap));

	for(i = HW_ISR_FIRST; i < HW_ISR_LINES; i++){
		if (vmid == 0) {
			switch(i) {
			case IRQ_VM1:
				irq_thdcap[i] = VM0_CAPTBL_SELF_IOTHD_SET_BASE;
				irq_thdid[i] = (thdid_t)cos_introspect(&booter_info, irq_thdcap[i], THD_GET_TID);
				irq_arcvcap[i] = VM0_CAPTBL_SELF_IORCV_SET_BASE;
				break;
			case IRQ_VM2:
				irq_thdcap[i] = VM0_CAPTBL_SELF_IOTHD_SET_BASE + CAP16B_IDSZ;
				irq_thdid[i] = (thdid_t)cos_introspect(&booter_info, irq_thdcap[i], THD_GET_TID);
				irq_arcvcap[i] = VM0_CAPTBL_SELF_IORCV_SET_BASE + CAP64B_IDSZ;
				break;
			default:
				irq_thdcap[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, cos_irqthd_handler, (void *)i);
				assert(irq_thdcap[i]);
				irq_thdid[i] = (thdid_t)cos_introspect(&booter_info, irq_thdcap[i], THD_GET_TID);
				assert(irq_thdid[i]);
				cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, 32 + i, irq_arcvcap[i]);
				break;
			}
		} else {
			switch(i) {
				case IRQ_DOM0_VM:
					irq_thdcap[i] = VM_CAPTBL_SELF_IOTHD_BASE;
					irq_thdid[i] = (thdid_t)cos_introspect(&booter_info, irq_thdcap[i], THD_GET_TID);
					irq_arcvcap[i] = VM_CAPTBL_SELF_IORCV_BASE;
					/* VMs use only 1 tcap - INITTCAP for all execution */
					irq_tcap[i] = BOOT_CAPTBL_SELF_INITTCAP_BASE;
					break;
				default:
					break;
			}
		}
	}
}

void
rump_booter_init(void)
{
	extern int vmid;

	char *json_file = "";
#define JSON_PAWS_BAREMETAL 0
#define JSON_PAWS_QEMU 1
#define JSON_NGINX_BAREMETAL 2
#define JSON_NGINX_QEMU 3

/* json config string fixed at compile-time */
#define JSON_CONF_TYPE JSON_PAWS_QEMU

	printc("~~~~~ vmid: %d ~~~~~\n", vmid);
	if(vmid == 0) {

#if JSON_CONF_TYPE == JSON_NGINX_QEMU
		json_file = "{,\"blk\":{,\"source\":\"dev\",\"path\":\"/dev/paws\",\"fstype\":\"cd9660\",\"mountpoint\":\"/data\",},\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"nginx.bin\",},\0";

#elif JSON_CONF_TYPE == JSON_NGINX_BAREMETAL
		json_file = "{,\"blk\":{,\"source\":\"dev\",\"path\":\"/dev/paws\",\"fstype\":\"cd9660\",\"mountpoint\":\"/data\",},\"net\":{,\"if\":\"wm0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"192.168.0.2\",\"mask\":\"24\",},\"cmdline\":\"nginx.bin\",},\0";
#elif JSON_CONF_TYPE == JSON_PAWS_QEMU
		json_file = "";
		//json_file = "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"paws.bin\",},\0";
		//json_file = "{,\"blk\":{,\"source\":\"dev\",\"path\":\"/dev/paws\",\"fstype\":\"cd9660\",\"mountpoint\":\"/data\",},\"cmdline\":\"paws.bin\",},\0";

#else /* JSON_CONF == JSON_PAWS_BAREMETAL */
		json_file = "{,\"net\":{,\"if\":\"wm0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"192.168.0.2\",\"mask\":\"24\",},\"cmdline\":\"paws.bin\",},\0";
#endif
	} else {

#if JSON_CONF_TYPE == JSON_NGINX_BAREMETAL
		/* VMs are just block devices */
		json_file = "{,\"blk\":{,\"source\":\"dev\",\"path\":\"/dev/paws\",\"fstype\":\"cd9660\",\"mountpoint\":\"/data\",},\"cmdline\":\"paws.bin\",},\0";
#endif
	}

#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	rk_thd_prio = PRIO_LOW;
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	rk_thd_prio = (vmid == 0) ? PRIO_BOOST : PRIO_UNDER;
#endif

	printc("\nRumpKernel Boot Start.\n");
	cos2rump_setup();
	/* possibly pass in the name of the program here to see if that fixes the name bug */

	printc("\nSetting up arcv for hw irq\n");
	hw_irq_alloc();
	//RK_hw_irq

	//bmk_isr_init(ipintr, NULL, 12);

	/* We pass in the json config string to the RK */
	cos_run(json_file);
	printc("\nRumpKernel Boot done.\n");

	cos_vm_exit();
	return;
}
