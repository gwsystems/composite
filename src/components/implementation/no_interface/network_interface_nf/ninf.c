#include "ninf.h"
#include "cos_eal_thd.h"
#include "sl.h"

struct cos_compinfo *ninf_info;
extern struct cos_pci_device devices[PCI_DEVICE_NUM];

#include <llprint.h>

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

void *
cos_map_phys_to_virt(void *paddr, unsigned int size)
{
	return (void *)cos_hw_map(ninf_info, BOOT_CAPTBL_SELF_INITHW_BASE, (paddr_t)paddr, size);
}

void
cos_init(void)
{
	ninf_info = cos_compinfo_get(cos_defcompinfo_curr_get());

	int argc = 3;
	char arg1[] = "DEBUG", arg2[] = "-l", arg3[] = "0";
	char *argv[] = {arg1, arg2, arg3};

	printc("\nDPDK EAL init started.\n");

	int ret = 0;
	ret = rte_eal_init(argc, argv);
	if (ret < 0) printc("Warning: rte_eal_init returned %d \n", ret);
	printc("\nDPDK EAL init done.\n");

	SPIN();

	return;
}
