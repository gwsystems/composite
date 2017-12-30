#include "dpdk_init.h"
#include "cos_eal_thd.h"
#include "sl.h"

struct cos_compinfo dpdk_init_info;
extern struct pci_device devices[PCI_DEVICE_NUM];

#include <llprint.h>


void
cos_init(void)
{
	cos_meminfo_init(&dpdk_init_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&dpdk_init_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
	                  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &dpdk_init_info);

    int argc = 1;
    char *argv[] = {"DEBUG"};

    printc("\nDPDK init started.\n");
    pci_init();
    int ret = 0;
    printc("\nCall to rte_eal_init returned %d \n", ret);
    ret = rte_eal_init(argc, argv);
	printc("\nDPDK init done.\n");

    SPIN();

	return;
}

cos_eal_thd_t
cos_eal_thd_curr(void)
{
    return (cos_eal_thd_t)(sl_thdid());
}

int
cos_eal_thd_create(cos_eal_thd_t *thd_id, void *(*func)(void *), void *arg)
{
    struct sl_thd *new_thd;
    new_thd = sl_thd_alloc((cos_thd_fn_t)func, arg);
    assert(new_thd);
    return new_thd->thdid;
}
