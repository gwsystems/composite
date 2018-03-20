#include "ninf.h"
#include "cos_eal_thd.h"
#include "sl.h"

#define NUM_MBUFS 8191
#define MBUF_SIZE 64
#define BURST_SIZE 32

extern struct cos_pci_device devices[PCI_DEVICE_NUM];
struct cos_compinfo *ninf_info;

u8_t nb_ports;
struct rte_mempool *mbuf_pool;

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

void *
cos_map_virt_to_phys(void *addr)
{
  int ret;
  vaddr_t vaddr = (vaddr_t)addr;

  assert((vaddr & 0xfff) == 0);
  ret = call_cap_op(cos_defcompinfo_curr_get()->ci.pgtbl_cap, CAPTBL_OP_INTROSPECT, (vaddr_t)vaddr, 0, 0, 0);
  return ret & 0xfffff000;
}

int
dpdk_init(void)
{
	/* Dummy argc and argv */
	int argc = 3;

	/* single core */
	char arg1[] = "DEBUG", arg2[] = "-l", arg3[] = "0";
	char *argv[] = {arg1, arg2, arg3};

	int ret;
	u8_t i;

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
	/* for (i = 0; i < nb_ports; i++) {} */
	if (rte_eth_dev_cos_setup_ports(nb_ports, mbuf_pool) < 0)
		return -2;
	printc("\nPort init done.\n");

	return 0;
}

void
cos_init(void)
{
	ninf_info = cos_compinfo_get(cos_defcompinfo_curr_get());
	int ret;
	struct rte_mbuf *mbufs_init[BURST_SIZE];
	struct rte_mbuf *mbufs[BURST_SIZE];
	u8_t port;
	int tot_rx = 0, tot_tx = 0;

	ret = dpdk_init();
	if (ret < 0) {
		printc("DPDK EAL init return error %d \n", ret);
	}

	port = 0;

	if (nb_ports < 2) {
		printc("Too few ports\n");
		goto halt;
	}

	if(rte_pktmbuf_alloc_bulk_cos(mbuf_pool, mbufs_init, BURST_SIZE)) {
		printc("Couldn't allocate packets\n");
		goto halt;
	}

	while(1) {
		/* Current logic is written for packets to be
		 * sent out one port and into the other on the same
		 * network card.
		 * TODO: write generic logic for testing
		 * TODO: free pkts so buffers don't overflow
		 * */
		const u16_t nb_tx = rte_eth_tx_burst_cos(port, 0, mbufs, BURST_SIZE);
		if (nb_tx == 0) continue;

		printc("Port %d sent %d packets\n", port, nb_tx);
		tot_tx += nb_tx;
		printc("Total TX: %d \n", tot_tx);

		port = !port;

		const u16_t nb_rx = rte_eth_rx_burst_cos(port, 0, mbufs, nb_tx);

		printc("Port %d received %d packets\n", port, nb_rx);
		tot_rx += nb_rx;
		printc("Total RX: %d \n", tot_rx);

		break;
	}

halt:
	SPIN();

	return;
}

