#include <llprint.h>
#include <cos_dpdk.h>
#include <nic.h>
#include <shm_bm.h>
#include <netshmem.h>
#include <sched.h>
#include <arpa/inet.h>

#include "interface_impl.h"

#define NB_RX_DESC_DEFAULT 1024
#define NB_TX_DESC_DEFAULT 1024

#define MAX_PKT_BURST 512

#define NIPQUAD(addr) \
((unsigned char *)&addr)[0], \
((unsigned char *)&addr)[1], \
((unsigned char *)&addr)[2], \
((unsigned char *)&addr)[3]
#define NIPQUAD_FMT "%u.%u.%u.%u"

struct eth_hdr {
	u8_t dst_addr[6];
	u8_t src_addr[6];
	u16_t ether_type;
} __attribute__((packed));

struct ether_addr {
	uint8_t addr_bytes[6]; /**< Addr bytes in tx order */
} __attribute__((packed));

struct arp_ipv4 {
	struct ether_addr arp_sha;  /**< sender hardware address */
	uint32_t          arp_sip;  /**< sender IP address */
	struct ether_addr arp_tha;  /**< target hardware address */
	uint32_t          arp_tip;  /**< target IP address */
} __attribute__((packed));

struct arp_hdr {
	uint16_t arp_hardware;    /* format of hardware address */
#define RTE_ARP_HRD_ETHER     1  /* ARP Ethernet address format */

	uint16_t arp_protocol;    /* format of protocol address */
	uint8_t  arp_hlen;    /* length of hardware address */
	uint8_t  arp_plen;    /* length of protocol address */
	uint16_t arp_opcode;     /* ARP opcode (command) */
#define	RTE_ARP_OP_REQUEST    1 /* request to resolve address */
#define	RTE_ARP_OP_REPLY      2 /* response to previous request */
#define	RTE_ARP_OP_REVREQUEST 3 /* request proto addr given hardware */
#define	RTE_ARP_OP_REVREPLY   4 /* response giving protocol address */
#define	RTE_ARP_OP_INVREQUEST 8 /* request to identify peer */
#define	RTE_ARP_OP_INVREPLY   9 /* response identifying peer */

	struct arp_ipv4 arp_data;
} __attribute__((packed));

struct ip_hdr {
	u8_t ihl:4;
	u8_t version:4;
	u8_t tos;
	u16_t total_len;
	u16_t id;
	u16_t frag_off;
	u8_t ttl;
	u8_t proto;
	u16_t checksum;
	u32_t src_addr;
	u32_t dst_addr;
} __attribute__((packed));

struct tcp_udp_port
{
	u16_t src_port;
	u16_t dst_port;
};

char* g_rx_mp = NULL;
char* g_tx_mp = NULL;

static u16_t nic_ports = 0;

static struct client_session *
find_session(uint32_t dst_ip, uint16_t dst_port)
{
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
		return;
	}

	shm_bm_free_net_pkt_buf(addr);
}

static void
process_tx_packets(void)
{
	struct pkt_buf buf;
	char* mbuf;

	while (!pkt_ring_buf_empty(g_tx_ring))
	{
		pkt_ring_buf_dequeue(g_tx_ring, g_tx_ringbuf, &buf);

		mbuf = cos_allocate_mbuf(g_tx_mp);
		cos_attach_external_mbuf(mbuf, buf.pkt, PKT_BUF_SIZE, ext_buf_free_callback_fn, buf.paddr);
		cos_send_external_packet(mbuf, buf.pkt_len);
	}
}

static void
process_rx_packets(cos_portid_t port_id, char** rx_pkts, uint16_t nb_pkts)
{
	int i;
	int len = 0;

	struct eth_hdr      *eth;
	struct ip_hdr       *iph;
	struct arp_hdr      *arp_hdr;
	struct tcp_udp_port *port;
	struct client_session * session;
	struct pkt_buf buf;

	for (i = 0; i < nb_pkts; i++) {
		char * pkt = cos_get_packet(rx_pkts[i], &len);
		eth = pkt;

		if (htons(eth->ether_type) == 0x0800) {
			iph = (char*)eth + sizeof(struct eth_hdr);
			port = (char*)eth + sizeof(struct eth_hdr) + iph->ihl * 4;

			session = find_session(iph->dst_addr, port->dst_port);
			if(session == NULL) {
				continue;
			}
			buf.pkt = rx_pkts[i];
			pkt_ring_buf_enqueue(session->ring, session->ringbuf, &buf);
			sched_thd_wakeup(session->thd);
		} else if (htons(eth->ether_type) == 0x0806) {
			arp_hdr = (char*)eth + sizeof(struct eth_hdr);
			session = find_session(arp_hdr->arp_data.arp_tip, 0);
			if(session == NULL) {
				continue;
			}
			buf.pkt = rx_pkts[i];
			pkt_ring_buf_enqueue(session->ring, session->ringbuf, &buf);
			
			sched_thd_wakeup(session->thd);
		} else {

			continue;
		}

	}
}

static void
cos_free_rx_buf()
{
	struct pkt_buf buf;
	char* mbuf;

	while (!pkt_ring_buf_empty(g_free_ring))
	{
		pkt_ring_buf_dequeue(g_free_ring, g_free_ringbuf, &buf);
		cos_free_packet(buf.pkt);
	}
}

static void
cos_nic_start(){
	int i;
	uint16_t nb_pkts = 0;

	char* rx_packets[MAX_PKT_BURST];

	while (1)
	{
		cos_free_rx_buf();
		/* infinite loop to process packets */
		for (i = 0; i < nic_ports; i++) {
			/* process rx */
			nb_pkts = cos_dev_port_rx_burst(i, 0, rx_packets, MAX_PKT_BURST);

			if (nb_pkts != 0) process_rx_packets(i, rx_packets, nb_pkts);

			process_tx_packets();
		}
	}
}

static void
cos_nic_init(void)
{
	int argc, ret, nb_pkts;

	#define MAX_PKT_BURST 512

	const char *rx_mpool_name = "cos_app_rx_pool";
	const char *tx_mpool_name = "cos_app_tx_pool";
	uint16_t i;
	uint16_t nb_rx_desc = NB_RX_DESC_DEFAULT;
	uint16_t nb_tx_desc = NB_TX_DESC_DEFAULT;

	/*
	 * set max_mbufs 2 times than nb_rx_desc, so that there is enough room
	 * to store packets, or this will fail if nb_rx_desc <= max_mbufs.
	 */
	const size_t max_mbufs = 2 * nb_rx_desc;

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
	char* mp = cos_create_pkt_mbuf_pool(rx_mpool_name, max_mbufs);
	assert(mp != NULL);
	g_rx_mp = mp;

	g_tx_mp = cos_create_pkt_mbuf_pool(tx_mpool_name, max_mbufs);
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
	pkt_ring_buf_init(&g_tx_ring, &g_tx_ringbuf, TX_PKT_RBUF_NUM, TX_PKT_RING_SZ);
	pkt_ring_buf_init(&g_free_ring, &g_free_ringbuf, FREE_PKT_RBUF_NUM, FREE_PKT_RING_SZ);

	printc("dpdk init end\n");
}

int
main(void)
{
	printc("NIC started\n");

	cos_nic_start();
	return 0;
}
