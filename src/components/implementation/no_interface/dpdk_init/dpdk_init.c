#include "dpdk_init.h"

struct cos_compinfo dpdk_init_info;
extern struct pci_device devices[PCI_DEVICE_NUM];

#include <llprint.h>

void
cos_init(void)
{
	cos_meminfo_init(&dpdk_init_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&dpdk_init_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
	                  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &dpdk_init_info);


    printc("\nDPDK init started.\n");
    pci_init();
	printc("\nDPDK init done.\n");

    SPIN();

	return;
}

