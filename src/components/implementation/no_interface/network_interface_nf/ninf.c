#include "ninf.h"
#include "cos_eal_thd.h"
#include "sl.h"

#define NUM_MBUFS 8191
#define MBUF_SIZE 64

extern struct cos_pci_device devices[PCI_DEVICE_NUM];
struct cos_compinfo *ninf_info;

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

int
dpdk_init(void)
{
	/* Dummy argc and argv */
	int argc = 3;
	char arg1[] = "DEBUG", arg2[] = "-l", arg3[] = "0";
	char *argv[] = {arg1, arg2, arg3};

	struct rte_mempool *mbuf_pool;
	int ret;
	u8_t i, nb_ports;

	printc("\nDPDK EAL init started.\n");
	ret = rte_eal_init(argc, argv);
	if (ret < 0) return ret;
	printc("\nDPDK EAL init done.\n");

	nb_ports = rte_eth_dev_count();
	if (!nb_ports) return -1;
	printc("%d ports available.\n", nb_ports);

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
			NUM_MBUFS * nb_ports, 0, 0, MBUF_SIZE, -1);
	if (!mbuf_pool) return -2;

	//TODO: Configure each port

	return 0;
}

void
cos_init(void)
{
	ninf_info = cos_compinfo_get(cos_defcompinfo_curr_get());
	int ret;

	ret = dpdk_init();
	if (ret < 0) {
		printc("DPDK EAL init return error %d \n", ret);
	}

	SPIN();

	return;
}
