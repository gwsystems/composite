#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "rumpcalls.h"
#include "cos_init.h"
#include "vk_api.h"
#include "cos_sync.h"

extern struct cos_compinfo booter_info;
extern int vmid;

u64_t t_vm_cycs  = 0;
u64_t t_dom_cycs = 0;

void
hw_irq_alloc(void){

	tcap_res_t budget;
	int i, ret;
	int first = 1, id = HW_ISR_FIRST;

	memset(irq_thdcap, 0, sizeof(irq_thdcap));
	memset(irq_thdid, 0, sizeof(irq_thdid));
	memset(irq_arcvcap, 0, sizeof(irq_arcvcap));
	memset(irq_tcap, 0, sizeof(irq_tcap));
	memset(irq_prio, 0, sizeof(irq_prio));

	for(i = HW_ISR_FIRST; i < HW_ISR_LINES; i++){
		if (vmid == 0) {
			switch(i) {
	//		case IRQ_VM1:
	//			intr_update(i, 0);
	//			irq_thdcap[i] = dom0_vio_thdcap(1);
	//			irq_thdid[i] = (thdid_t)cos_introspect(&booter_info, irq_thdcap[i], THD_GET_TID);
	//			irq_arcvcap[i] = dom0_vio_rcvcap(1);
	//			irq_tcap[i] = dom0_vio_tcap(1);
	//			irq_prio[i] = PRIO_MID;
	//			break;
	//		case IRQ_VM2:
	//			if (COS2RK_VIRT_MACH_COUNT ==3) {
	//				intr_update(i, 0);
	//				irq_thdcap[i] = dom0_vio_thdcap(2);
	//				irq_thdid[i] = (thdid_t)cos_introspect(&booter_info, irq_thdcap[i], THD_GET_TID);
	//				irq_arcvcap[i] = dom0_vio_rcvcap(2);
	//				irq_tcap[i] = dom0_vio_tcap(2);
	//				irq_prio[i] = PRIO_MID;
	//			}
	//			break;
			default:
				intr_update(i, 0);
				irq_thdcap[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, cos_irqthd_handler, (void *)i);
				assert(irq_thdcap[i]);
				irq_thdid[i] = (thdid_t)cos_introspect(&booter_info, irq_thdcap[i], THD_GET_TID);
				assert(irq_thdid[i]);
				irq_prio[i] = PRIO_MID;

				irq_tcap[i] = BOOT_CAPTBL_SELF_INITTCAP_BASE;
				irq_arcvcap[i] = cos_arcv_alloc(&booter_info, irq_thdcap[i], BOOT_CAPTBL_SELF_INITTCAP_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
				assert(irq_arcvcap[i]);
				cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, 32 + i, irq_arcvcap[i]);
				break;
			}
		} else {
//			switch(i) {
//				case IRQ_DOM0_VM:
//					intr_update(i, 0);
//					irq_thdcap[i] = VM_CAPTBL_SELF_IOTHD_BASE;
//					irq_thdid[i] = (thdid_t)cos_introspect(&booter_info, irq_thdcap[i], THD_GET_TID);
//					irq_arcvcap[i] = VM_CAPTBL_SELF_IORCV_BASE;
//					/* VMs use only 1 tcap - INITTCAP for all execution */
//					irq_tcap[i] = BOOT_CAPTBL_SELF_INITTCAP_BASE;
//					irq_prio[i] = PRIO_MID;
//					break;
//				default:
//					break;
//			}
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
#define JSON_UPDSERV_BAREMETAL JSON_PAWS_BAREMETAL
#define JSON_UDPSERV_QEMU JSON_PAWS_QEMU
#define JSON_NGINX_BAREMETAL 2
#define JSON_NGINX_QEMU 3

/* json config string fixed at compile-time */
//#define JSON_CONF_TYPE JSON_UDPSERV_BAREMETAL
//#define JSON_CONF_TYPE JSON_PAWS_BAREMETAL
#define JSON_CONF_TYPE JSON_PAWS_QEMU

	printc("~~~~~ vmid: %d ~~~~~\n", vmid);
	assert(vmid == 0);
	if(vmid == 0) {

#if JSON_CONF_TYPE == JSON_NGINX_QEMU
		json_file = "{,\"blk\":{,\"source\":\"dev\",\"path\":\"/dev/paws\",\"fstype\":\"cd9660\",\"mountpoint\":\"/data\",},\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"nginx.bin\",},\0";

#elif JSON_CONF_TYPE == JSON_NGINX_BAREMETAL
		json_file = "{,\"blk\":{,\"source\":\"dev\",\"path\":\"/dev/paws\",\"fstype\":\"cd9660\",\"mountpoint\":\"/data\",},\"net\":{,\"if\":\"wm0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"192.168.0.2\",\"mask\":\"24\",},\"cmdline\":\"nginx.bin\",},\0";
#elif JSON_CONF_TYPE == JSON_PAWS_QEMU
		json_file = "{,\"net\":{,\"if\":\"vioif0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"10.0.120.101\",\"mask\":\"24\",},\"cmdline\":\"paws.bin\",},\0";

#else /* JSON_CONF == JSON_PAWS_BAREMETAL */
		json_file = "{,\"net\":{,\"if\":\"wm0\",\"type\":\"inet\",\"method\":\"static\",\"addr\":\"192.168.0.2\",\"mask\":\"24\",},\"cmdline\":\"paws.bin\",},\0";
#endif
	} else {

#if JSON_CONF_TYPE == JSON_NGINX_BAREMETAL
		/* VMs are just block devices */
		json_file = "{,\"blk\":{,\"source\":\"dev\",\"path\":\"/dev/paws\",\"fstype\":\"cd9660\",\"mountpoint\":\"/data\",},\"cmdline\":\"paws.bin\",},\0";
#endif
	}

	rk_thd_prio = PRIO_MID;

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
