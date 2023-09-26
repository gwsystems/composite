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
#include <sync_lock.h>
#include "nicmgr.h"

#define ENABLE_DEBUG_INFO 0

#define NB_RX_DESC_DEFAULT 2048
#define NB_TX_DESC_DEFAULT 1024

#define MAX_PKT_BURST NB_RX_DESC_DEFAULT

unsigned long enqueued_rx = 0, dequeued_tx = 0;
int debug_flag = 0;

/* rx and tx stats */
u64_t rx_enqueued_miss = 0;
extern rte_atomic64_t tx_enqueued_miss;

char *g_rx_mp[NIC_RX_QUEUE_NUM];
char *g_tx_mp[NIC_TX_QUEUE_NUM];
char rx_per_core_mpool_name[NIC_RX_QUEUE_NUM][32];
char tx_per_core_mpool_name[NIC_TX_QUEUE_NUM][32];

static u16_t nic_ports = 0;

struct rte_hash *tenant_hash_tbl;
struct sync_lock tx_lock[NIC_TX_QUEUE_NUM];

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

struct client_session *
cos_hash_lookup(uint16_t tenant_id)
{
	int ret;
	struct client_session *session = NULL;

	/* If DPDK receives this port, it goes to process debug information */
	if (unlikely(tenant_id == htons(NIC_DEBUG_PORT))) {
		// asm volatile("mfence");
		printc("debug flag is open\n");
		debug_flag = 1;
		return NULL;
	}

	ret = rte_hash_lookup_data(tenant_hash_tbl, &tenant_id, (void *)&session);
	if (ret < 0) return NULL;

	return session;
}

static void
debug_print_stats(void)
{
	cos_get_port_stats(0);

	printc("rx mempool in use:%u\n", cos_mempool_in_use_count(g_rx_mp[0]));
	printc("rx enqueued miss:%llu\n", rx_enqueued_miss);
	printc("tx enqueued miss:%lu\n", tx_enqueued_miss.cnt);
	printc("enqueue:%lu, txqneueue:%lu\n", enqueued_rx, dequeued_tx);
	struct client_session	*session1, *session2;
	session1 = cos_hash_lookup(ntohs(6));
	session2 = cos_hash_lookup(ntohs(7));
	int ret1 = sched_debug_thd_state(session1->thd);
	int ret2 = sched_debug_thd_state(session2->thd);
	printc("com 6:%u\n", ret1);
	printc("com 7:%u\n", ret2);
}

static void
debug_dump_info(void)
{
	static u64_t counter = 0;
	#define LIMIT 1000000000
	counter++;
	if (counter > LIMIT) {
		debug_print_stats();
		counter=0;
	}
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
	// cycles_t    before, after;
	for (j = 0; j < NIC_MAX_SESSION; j++) {
		if (client_sessions[j].tx_init_done == 0) continue;

		per_thd_tx_ring = &client_sessions[j].pkt_tx_ring;
		while (!pkt_ring_buf_empty(per_thd_tx_ring)) {
			i = 0;

			pkt_ring_buf_dequeue(per_thd_tx_ring, &buf);

			dequeued_tx++;
			mbuf = cos_allocate_mbuf(g_tx_mp[0]);
			assert(mbuf);
			ext_shinfo = netshmem_get_tailroom((struct netshmem_pkt_buf *)buf.obj);

			cos_attach_external_mbuf(mbuf, buf.obj, buf.paddr, PKT_BUF_SIZE, ext_buf_free_callback_fn, ext_shinfo);
			cos_set_external_packet(mbuf, (buf.pkt - buf.obj), buf.pkt_len, ENABLE_OFFLOAD);
			tx_packets[i++] = mbuf;
			// rdtscll(before);
			cos_dev_port_tx_burst(0, 0, tx_packets, i);
			// rdtscll(after);
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
				// assert(0);
				cos_free_packet(rx_pkts[i]);
				rx_enqueued_miss++;
				continue;
			}
			port	= (struct tcp_udp_port *)((char *)eth + sizeof(struct eth_hdr) + iph->ihl * 4);

			// printc("port id:0x%x\n", port->dst_port);
			session = cos_hash_lookup(port->dst_port);
			// assert(session->port == port->dst_port);
			if (unlikely(debug_flag)) {
				debug_print_stats();
				debug_flag = 0;
			}
			if (unlikely(session == NULL)) {
				cos_free_packet(rx_pkts[i]);
				continue;
			}
			// memset(&buf, 0, sizeof(buf));
			buf.pkt = rx_pkts[i];
			// if (ntohs(port->dst_port) == 6) {
			// 	buf.tent_id = 6;
			// 	buf.srcid = ntohs(port->src_port);
			// } else if(ntohs(port->dst_port) == 7) {
			// 	buf.tent_id = 7;
			// 	buf.srcid = ntohs(port->src_port);
			// } else {
			// 	assert(0);
			// }
			if (unlikely(!pkt_ring_buf_enqueue(&(session->pkt_ring_buf), &buf))){
				cos_free_packet(buf.pkt);
				rx_enqueued_miss++;
				continue;
			}
			enqueued_rx++;
			// if (unlikely(debug_flag)) {
			// 	session->sem.debug = 1;
			// }

			sync_sem_give(&session->sem);
		} else if (htons(eth->ether_type) == 0x0806) {
			cos_free_packet(buf.pkt);
			continue;
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
swap_mac(struct eth_hdr *eth) {
	char tmp[6];
	memcpy(tmp, &eth->dst_addr.addr_bytes[0], 6);
	memcpy(&eth->dst_addr.addr_bytes[0], &eth->src_addr.addr_bytes[0], 6);
	memcpy(&eth->src_addr.addr_bytes[0], tmp, 6);
}

static void
transmit_back(cos_portid_t port_id, char** rx_pkts, uint16_t nb_pkts)
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
		swap_mac(eth);
	}
	// printc("tx :%d\n", nb_pkts);
	ret = cos_dev_port_tx_burst(0, 0, rx_pkts, nb_pkts);
	assert(ret == nb_pkts);
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
#if ENABLE_DEBUG_INFO
		debug_dump_info();
#endif
		// process_tx_packets();

		// only port 0, queue 0 receive packets
		nb_pkts = cos_dev_port_rx_burst(0, 0, rx_packets, MAX_PKT_BURST);
		/* These are the two test options */
		// if (nb_pkts!= 0) transmit_back(0, rx_packets, nb_pkts);
		// if (nb_pkts!= 0) cos_dev_port_tx_burst(0, 0, rx_packets, nb_pkts);

		/* This is the real processing logic for applications */
		if (nb_pkts != 0) process_rx_packets(0, rx_packets, nb_pkts);
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
	memset(rx_per_core_mpool_name, 0, sizeof(rx_per_core_mpool_name));
	memset(tx_per_core_mpool_name, 0, sizeof(tx_per_core_mpool_name));

	char *argv[] =	{
			"COS_DPDK_BOOTER", /* single core, the first argument has to be the program name */
			"-l", 
			"0",
			"--no-shconf", 
			"--no-huge",
			"--iova-mode=pa",
			"--log-level",
			"*:debug", /* log level can be changed to *debug* if needed, this will print lots of information */
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
	for (i = 0; i < NIC_RX_QUEUE_NUM; i++) {
		rx_per_core_mpool_name[i][0] = 'r';
		rx_per_core_mpool_name[i][1] = i;
		g_rx_mp[i] = cos_create_pkt_mbuf_pool_by_ops(rx_per_core_mpool_name[i], max_rx_mbufs, COS_MEMPOOL_MT_RTS_OPS);
		assert(g_rx_mp[i]);
	}

	for(i = 0;i < NIC_TX_QUEUE_NUM; i++) {
		tx_per_core_mpool_name[i][0] = 'p';
		tx_per_core_mpool_name[i][1] = i;
		g_tx_mp[i] = cos_create_pkt_mbuf_pool_by_ops(tx_per_core_mpool_name[i], max_tx_mbufs, COS_MEMPOOL_MT_RTS_OPS);
		assert(g_tx_mp[i]);
	}


	/* 4. config each port */
	for (i = 0; i < nic_ports; i++) {
		cos_config_dev_port_queue(i, NIC_RX_QUEUE_NUM, NIC_TX_QUEUE_NUM);
		cos_dev_port_adjust_rx_tx_desc(i, &nb_rx_desc, &nb_tx_desc);
		for (int j = 0; j < NIC_RX_QUEUE_NUM; j++) {
			cos_dev_port_rx_queue_setup(i, j, nb_rx_desc, g_rx_mp[j]);
		}
		for (int j = 0; j < NIC_TX_QUEUE_NUM; j++) {
			cos_dev_port_tx_queue_setup(i, j, nb_tx_desc);
		}
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
#ifdef USE_CK_RING_FREE_MBUF
	pkt_ring_buf_init(&g_free_ring, FREE_PKT_RBUF_NUM, FREE_PKT_RING_SZ);
#endif
	for (int i = 0;i < NIC_TX_QUEUE_NUM;i++) {
		sync_lock_init(&tx_lock[i]);
	}

	printc("dpdk init end\n");
}

int
parallel_main(coreid_t cid)
{
	/* DPDK rx and tx will only run on core 0 */
	if(cid == 0) {
		cos_nic_start();
	} else {
#if 0
#if E810_NIC == 0
		sync_lock_take(&tx_lock[0]);
		cos_test_send(0, g_tx_mp[0]);
		sync_lock_release(&tx_lock[0]);
#else
		cos_test_send(cid - 1, g_tx_mp[cid - 1]);
#endif
#endif
	}

	return 0;
}
