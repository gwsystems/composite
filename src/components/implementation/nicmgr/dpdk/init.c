#include <llprint.h>
#include <cos_dpdk.h>
#include <nic.h>
#include <shm_bm.h>
#include <netshmem.h>
#include <sched.h>
#include <arpa/inet.h>
#include <net_stack_types.h>
#include <rte_atomic.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_hash_crc.h>
#include <sync_blkpt.h>
#include "nicmgr.h"

#define NB_RX_DESC_DEFAULT 1024
#define NB_TX_DESC_DEFAULT 1024

#define MAX_PKT_BURST NB_RX_DESC_DEFAULT

unsigned long enqueued_rx = 0, dequeued_tx = 0;
int debug_flag = 0;

/* rx and tx stats */
u64_t rx_enqueued_miss = 0;
extern rte_atomic64_t tx_enqueued_miss;

char *g_rx_mp = NULL;
char *g_tx_mp = NULL;

static u16_t nic_ports = 0;
static u16_t nic_queues = 1;

struct rte_hash *tenant_hash_tbl;

static struct rte_hash_parameters rte_hash_params = {
	.entries = NIC_MAX_SESSION,
	.key_len = sizeof(uint16_t),
	.hash_func = rte_jhash,
	.hash_func_init_val = 0,
	.socket_id = 0,
	.extra_flag = RTE_HASH_EXTRA_FLAGS_MULTI_WRITER_ADD,
};

static void
cos_hash_init()
{
	int pos, ret;

	rte_hash_params.name = "tenant_id_tbl";

	tenant_hash_tbl = rte_hash_create(&rte_hash_params);

	assert(tenant_hash_tbl);

	printc("tenant hash table init done\n");
}

void 
cos_hash_add(uint16_t tenant_id, struct client_session *session)
{
	int ret;
	ret = rte_hash_add_key_data(tenant_hash_tbl, &tenant_id, session);
	assert(ret >= 0);
}

static void
debug_print_stats(void)
{
	cos_get_port_stats(0);

	printc("rx mempool in use:%u\n", cos_mempool_in_use_count(g_rx_mp));
	printc("rx enqueued miss:%llu\n", rx_enqueued_miss);
	printc("tx enqueued miss:%lu\n", tx_enqueued_miss.cnt);
	printc("enqueue:%lu, txqneueue:%lu\n", enqueued_rx, dequeued_tx);
}

struct client_session *
cos_hash_lookup(uint16_t tenant_id)
{
	int ret;
	struct client_session *session;

	/* If DPDK receives this port, it goes to process debug information */
	if (unlikely(tenant_id == htons(NIC_DEBUG_PORT))) {
		debug_flag = 1;
		return NULL;
	}

	ret = rte_hash_lookup_data(tenant_hash_tbl, &tenant_id, (void *)&session);
	assert(ret >= 0);

	return session;
}

static void
ext_buf_free_callback_fn(void *addr, void *opaque)
{
	/* Shared mem uses borrow api, thus do not need to free it here */
	if (addr == NULL) {
		printc("External buffer address is invalid\n");
		assert(0);
	}
}

static void
process_tx_packets(void)
{
	struct pkt_buf buf;
	char *mbuf;
	void *ext_shinfo;
	char *tx_packets[MAX_PKT_BURST];
	struct pkt_ring_buf *per_thd_tx_ring;
	int i = 0, j = 0;

	for (j = 0; j < NIC_MAX_SESSION; j++) {
		if (client_sessions[j].tx_init_done == 0) continue;

		per_thd_tx_ring = &client_sessions[j].pkt_tx_ring;
		while (!pkt_ring_buf_empty(per_thd_tx_ring)) {
			i = 0;

			pkt_ring_buf_dequeue(per_thd_tx_ring, &buf);

			dequeued_tx++;
			mbuf = cos_allocate_mbuf(g_tx_mp);
			assert(mbuf);
			ext_shinfo = netshmem_get_tailroom((struct netshmem_pkt_buf *)buf.obj);

			cos_attach_external_mbuf(mbuf, buf.obj, buf.paddr, PKT_BUF_SIZE, ext_buf_free_callback_fn, ext_shinfo);
			cos_set_external_packet(mbuf, (buf.pkt - buf.obj), buf.pkt_len, ENABLE_OFFLOAD);
			tx_packets[i++] = mbuf;

			cos_dev_port_tx_burst(0, 0, tx_packets, i);
		}

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
			if (unlikely(iph->proto != UDP_PROTO)) {
				cos_free_packet(buf.pkt);
				rx_enqueued_miss++;
				continue;
			}
			port	= (struct tcp_udp_port *)((char *)eth + sizeof(struct eth_hdr) + iph->ihl * 4);

			session = cos_hash_lookup(port->dst_port);
			if (unlikely(session == NULL)) {
				cos_free_packet(rx_pkts[i]);
				continue;
			}

			buf.pkt = rx_pkts[i];
			if (unlikely(!pkt_ring_buf_enqueue(&session->pkt_ring_buf, &buf))){
				cos_free_packet(buf.pkt);
				rx_enqueued_miss++;
				continue;
			}
			enqueued_rx++;

			sync_sem_give(&session->sem);

			if (unlikely(debug_flag)) {
				debug_print_stats();
				debug_flag = 0;
			}
		} else if (htons(eth->ether_type) == 0x0806) {
			assert(0);
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
	int i, j, recv_round;
	uint16_t nb_pkts = 0;

	char *rx_packets[MAX_PKT_BURST];

	while (1) {
#if USE_CK_RING_FREE_MBUF
		cos_free_rx_buf();
#endif
		process_tx_packets();
		for (i = 0; i < nic_ports; i++) {
			for (j = 0; j < nic_queues; j++) {
				nb_pkts = cos_dev_port_rx_burst(i, j, rx_packets, MAX_PKT_BURST);
				if (nb_pkts != 0) process_rx_packets(i, rx_packets, nb_pkts);
			}
		}
	}
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
	char *mp = cos_create_pkt_mbuf_pool_by_ops(rx_mpool_name, max_rx_mbufs, COS_MEMPOOL_MT_RTS_OPS);
	assert(mp != NULL);
	g_rx_mp = mp;

	g_tx_mp = cos_create_pkt_mbuf_pool_by_ops(tx_mpool_name, max_tx_mbufs, COS_MEMPOOL_MT_RTS_OPS);
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
	cos_hash_init();

	pkt_ring_buf_init(&g_free_ring, FREE_PKT_RBUF_NUM, FREE_PKT_RING_SZ);

	printc("dpdk init end\n");
}

int
parallel_main(coreid_t cid)
{
	/* DPDK rx and tx will only run on core 0 */
	if(cid != 0) return 0;
	printc("NIC started:%ld\n", cos_thdid());

	cos_nic_start();
	return 0;
}
