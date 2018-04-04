#include "ninf.h"
#include "ninf_util.h"

#define RX_RING_SIZE    512
#define TX_RING_SIZE    512
#define NUM_MBUFS       8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE      32

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;

	if (port >= rte_eth_dev_count()) return -1;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0) return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0) return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0) return retval;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), NULL);
		if (retval < 0) return retval;
	}

	/* Start the Ethernet port. */
	retval = rte_eth_dev_start(port);
	if (retval < 0) return retval;

	/* Display the port MAC address. */
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printc("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	rte_eth_promiscuous_enable(port);

	return 0;
}

int
rte_eth_dev_cos_setup_ports(unsigned nb_ports,
		struct rte_mempool *mp)
{
	uint8_t portid;
	for (portid = 0; portid < nb_ports; portid++) {
		if (port_init(portid, mp) != 0) {
			return -1;
		}
	}

	return 0;
}

void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t portid, count, all_ports_up, print_flag = 0;
	struct rte_eth_link link;

	/* printc("\nChecking link status"); */
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << portid)) == 0) continue;
			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(portid, &link);
			/* print link status if flag set */
			if (print_flag == 1) {
				if (link.link_status) {
					printc("Port %d Link Up - speed %u "
					       "Mbps - %s\n", (uint8_t)portid,
					       (unsigned)link.link_speed,
					       (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					       ("full-duplex") : ("half-duplex\n"));
				} else {
					printc("Port %d Link Down\n", (uint8_t)portid);
				}
				continue;
			}
			/* clear all_ports_up flag if any link down */
			if (link.link_status == ETH_LINK_DOWN) {
				all_ports_up = 0;
				break;
			}
		}
		/* after finally printing all link status, get out */
		if (print_flag == 1) break;

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		}

		/* set the print_flag if all ports up or timeout */
		if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
			print_flag = 1;
			printc("done\n");
		}
	}
}

void
print_ether_addr(struct rte_mbuf *m)
{
	struct ether_hdr *eth_hdr;
	char buf[ETHER_ADDR_FMT_SIZE];

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, &eth_hdr->s_addr);
	printc("src MAX: %s ", buf);
	ether_format_addr(buf, ETHER_ADDR_FMT_SIZE, &eth_hdr->d_addr);
	printc("dst MAC: %s\n" ,buf);
}
