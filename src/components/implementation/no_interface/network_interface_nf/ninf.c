#include "ninf.h"
#include "cos_eal_thd.h"
#include "sl.h"
#include "ninf_util.h"

#define NUM_MBUFS 8192
#define BURST_SIZE 32
#define RX_MBUF_DATA_SIZE 2048
#define MBUF_SIZE (RX_MBUF_DATA_SIZE + RTE_PKTMBUF_HEADROOM + sizeof(struct rte_mbuf))
#define N_LOOP 2

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

	ret = rte_eal_init(argc, argv);
	if (ret < 0) return ret;
	/* printc("\nDPDK EAL init done.\n"); */

	nb_ports = rte_eth_dev_count();
	if (!nb_ports) return -1;
	/* printc("%d ports available.\n", nb_ports); */

	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports, 0, 0, MBUF_SIZE, -1);
	if (!mbuf_pool) return -2;

	//TODO: Configure each port
	/* for (i = 0; i < nb_ports; i++) {} */
	if (rte_eth_dev_cos_setup_ports(nb_ports, mbuf_pool) < 0)
		return -2;
	/* printc("\nPort init done.\n"); */

	return 0;
}

int g_dbg = -1;
void
cos_init(void)
{
	ninf_info = cos_compinfo_get(cos_defcompinfo_curr_get());
	printc("dbg 0 ninf info %p\n", ninf_info);
	int ret, i;
	/* struct rte_mbuf *mbufs_init[BURST_SIZE]; */
	struct rte_mbuf *mbufs[BURST_SIZE];
	u8_t port;
	int tot_rx = 0, tot_tx = 0;

	ret = dpdk_init();
	if (ret < 0) {
		printc("DPDK EAL init return error %d \n", ret);
		goto halt;
	}
	if (nb_ports < 2) {
		printc("Too few ports\n");
		goto halt;
	}

	/* if(rte_pktmbuf_alloc_bulk_cos(mbuf_pool, mbufs_init, BURST_SIZE)) { */
	/* 	printc("Couldn't allocate packets\n"); */
	/* 	goto halt; */
	/* } */
	check_all_ports_link_status(nb_ports, 3);

	/* for(port = 0, i=0; i < N_LOOP; i++) { */
	while (1) {
		/* Current logic is written for packets to be
		 * sent out one port and into the other on the same
		 * network card.
		 * TODO: write generic logic for testing
		 * TODO: free pkts so buffers don't overflow
		 * */
		/* port = !port; */
		for(port=0; port<2; port++) {
			const u16_t nb_rx = rte_eth_rx_burst(port, 0, mbufs, BURST_SIZE);
			tot_rx += nb_rx;
			/* printc("Total RX: %d %d\n", tot_rx, g_dbg); */

			/* struct ether_hdr *ehdr; */
			/* mbufs_init[0] = rte_pktmbuf_alloc(mbuf_pool); */
			/* ehdr = (struct ether_hdr *)rte_pktmbuf_append(mbufs_init[0], ETHER_HDR_LEN); */
			/* rte_eth_macaddr_get(0, &ehdr->s_addr); */
			/* rte_eth_macaddr_get(0, &ehdr->d_addr); */
			/* ehdr->ether_type = ETHER_TYPE_IPv4; */
			/* mbufs_init[0]->port = port; */
			if (nb_rx) {
				/* printc("Port %d received %d packets\n", port, nb_rx); */
				/* rte_pktmbuf_dump(stdout, mbufs[0], 20); */
				/* printc("dbg ninf p %d pl %d dl %d\n", mbufs[0]->port, mbufs[0]->pkt_len, mbufs[0]->data_len); */
				/* printc("Total RX: %d \n", tot_rx); */
				/* print_ether_addr(mbufs[0]); */
				const u16_t nb_tx = rte_eth_tx_burst(!port, 0, mbufs, nb_rx);
				assert(nb_tx != 0);
				/* printc("Port %d sent %d packets d %d\n", port, nb_tx, g_dbg); */

				/* printc("Port %d sent %d packets\n", !port, nb_tx); */
				tot_tx += nb_tx;
				/* printc("Total TX: %d \n", tot_tx); */
			}
		}
	}
	printc("going to SPIN\n");

halt:
	SPIN();

	return;
}

