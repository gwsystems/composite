#include <llprint.h>
#include <cos_dpdk.h>
#include <nic.h>
#include <shm_bm.h>
#include <netshmem.h>
#include <sched.h>
#include <arpa/inet.h>
#include <cos_headers.h>
#include "nicmgr.h"

#define NB_RX_DESC_DEFAULT 1024
#define NB_TX_DESC_DEFAULT 1024

#define MAX_PKT_BURST NB_RX_DESC_DEFAULT

char *g_rx_mp = NULL;
char *g_tx_mp = NULL;

static u16_t nic_ports = 0;

static inline struct client_session *
find_session(uint32_t dst_ip, uint16_t dst_port)
{
	/* TODO: change this search to hash-table in next version */
	int i;
	for (i = 0; i < NIC_MAX_SESSION; i++) {
		if (client_sessions[i].ip_addr == dst_ip /*&& client_sessions[i].port == dst_port*/) {
			return &client_sessions[i];
		}
	}

	return NULL;
}

static void
ext_buf_free_callback_fn(void *addr, void *opaque)
{
	if (addr == NULL) {
		printc("External buffer address is invalid\n");
		assert(0);
	}

	shm_bm_free_net_pkt_buf(addr);
}

static void
process_tx_packets(void)
{
	struct pkt_buf buf;
	char *mbuf;
	void *ext_shinfo;

	while (!pkt_ring_buf_empty(&g_tx_ring)) {
		pkt_ring_buf_dequeue(&g_tx_ring, &buf);

		mbuf = cos_allocate_mbuf(g_tx_mp);
		assert(mbuf);
		ext_shinfo = netshmem_get_tailroom((struct netshmem_pkt_buf *)buf.obj);

		cos_attach_external_mbuf(mbuf, buf.obj, buf.paddr, PKT_BUF_SIZE, ext_buf_free_callback_fn, ext_shinfo);
		cos_send_external_packet(mbuf, (buf.pkt - buf.obj), buf.pkt_len);
	}
}

static void
process_rx_packets(cos_portid_t port_id, char** rx_pkts, uint16_t nb_pkts)
{
	int i, ret;
	int len = 0;

	struct eth_hdr		*eth;
	struct ip_hdr		*iph;
	struct arp_hdr		*arp_hdr;
	struct tcp_udp_port	*port;
	struct client_session	*session;
	char                    *pkt;
	struct pkt_buf           buf;

	for (i = 0; i < nb_pkts; i++) {
		pkt = cos_get_packet(rx_pkts[i], &len);
		eth = (struct eth_hdr *)pkt;

		if (htons(eth->ether_type) == 0x0800) {
			iph	= (struct ip_hdr *)((char *)eth + sizeof(struct eth_hdr));
			port	= (struct tcp_udp_port *)((char *)eth + sizeof(struct eth_hdr) + iph->ihl * 4);

			session = find_session(iph->dst_addr, port->dst_port);
			if (session == NULL) continue;

			buf.pkt = rx_pkts[i];
			if (!pkt_ring_buf_enqueue(&session->pkt_ring_buf, &buf)){
				cos_free_packet(buf.pkt);
				continue;
			}

			sched_thd_wakeup(session->thd);
		} else if (htons(eth->ether_type) == 0x0806) {
			arp_hdr = (struct arp_hdr *)((char *)eth + sizeof(struct eth_hdr));

			session = find_session(arp_hdr->arp_data.arp_tip, 0);
			if (session == NULL) continue;

			buf.pkt = rx_pkts[i];
			pkt_ring_buf_enqueue(&session->pkt_ring_buf, &buf);

			sched_thd_wakeup(session->thd);
		}
	}
}

static void
cos_free_rx_buf()
{
	int            ret;
	struct pkt_buf buf;
	char          *mbuf;

	while (!pkt_ring_buf_empty(&g_free_ring)) {
		ret = pkt_ring_buf_dequeue(&g_free_ring, &buf);
		assert(ret);
		cos_free_packet(buf.pkt);
	}
}

static void
cos_nic_start(){
	int i;
	uint16_t nb_pkts = 0;

	char *rx_packets[MAX_PKT_BURST];

	while (1) {
		/* infinite loop to process packets */
		cos_free_rx_buf();
		process_tx_packets();
		for (i = 0; i < nic_ports; i++) {
			/* process rx */
			nb_pkts = cos_dev_port_rx_burst(i, 0, rx_packets, MAX_PKT_BURST);

			if (nb_pkts != 0) process_rx_packets(i, rx_packets, nb_pkts);
		}
	}

	cos_get_port_stats(0);
}

static void
cos_nic_init(void)
{
	int argc, ret, nb_pkts;

	const char *rx_mpool_name = "cos_app_rx_pool";
	const char *tx_mpool_name = "cos_app_tx_pool";

	uint16_t i;
	uint16_t nb_rx_desc = NB_RX_DESC_DEFAULT;
	uint16_t nb_tx_desc = NB_TX_DESC_DEFAULT;

	/*
	 * set max_mbufs 2 times than nb_rx_desc, so that there is enough room
	 * to store packets, or this will fail if nb_rx_desc <= max_mbufs.
	 */
	const size_t max_rx_mbufs = 8 * nb_rx_desc;
	const size_t max_tx_mbufs = 2 * nb_tx_desc;

	char *argv[] =	{
			"COS_DPDK_BOOTER", /* single core, the first argument has to be the program name */
			"-l", 
			"0",
			"--no-shconf", 
			"--no-huge",
			"--iova-mode=pa",
			"--log-level",
			"*:info", /* log level can be changed to *debug* if needed, this will print lots of information */
			"-m",
			"128", /* total memory used by dpdk memory subsystem, such as mempool */
			};

	argc = ARRAY_SIZE(argv);

	/* 1. do initialization of dpdk */
	ret = cos_dpdk_init(argc, argv);

	assert(ret == argc - 1); /* program name is excluded */

	/* 2. init all Ether ports */
	nic_ports = cos_eth_ports_init();

	assert(nic_ports > 0);

	/* 3. create mbuf pool where packets will be stored, user can create multiple pools */
	char *mp = cos_create_pkt_mbuf_pool(rx_mpool_name, max_rx_mbufs);
	assert(mp != NULL);
	g_rx_mp = mp;

	g_tx_mp = cos_create_pkt_mbuf_pool(tx_mpool_name, max_tx_mbufs);
	assert(g_tx_mp);

	/* 4. config each port */
	for (i = 0; i < nic_ports; i++) {
		cos_config_dev_port_queue(i, 1, 1);
		cos_dev_port_adjust_rx_tx_desc(i, &nb_rx_desc, &nb_tx_desc);
		cos_dev_port_rx_queue_setup(i, 0, nb_rx_desc, g_rx_mp);
		cos_dev_port_tx_queue_setup(i, 0, nb_tx_desc);
	}

	/* 5. start each port, this will enable rx/tx */
	for (i = 0; i < nic_ports; i++) {
		cos_dev_port_start(i);
		cos_dev_port_set_promiscuous_mode(i, COS_DPDK_SWITCH_ON);
	}
}

void
cos_init(void)
{
	printc("nicmgr init...\n");
	cos_nic_init();
	pkt_ring_buf_init(&g_tx_ring, TX_PKT_RBUF_NUM, TX_PKT_RING_SZ);
	pkt_ring_buf_init(&g_free_ring, FREE_PKT_RBUF_NUM, FREE_PKT_RING_SZ);

	printc("dpdk init end\n");
}

int
main(void)
{
	printc("NIC started\n");

	cos_nic_start();
	return 0;
}
