#include <llprint.h>
#include <cos_dpdk.h>
#include <nic.h>

#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/tcp.h>
#include <lwip/stats.h>
#include <lwip/prot/tcp.h>

#define NB_RX_DESC_DEFAULT 1024
#define NB_TX_DESC_DEFAULT 1024
#define MAX_PKT_BURST 512

static struct ip4_addr ip, mask, gw;
static struct netif cos_interface;

static u16_t nic_ports = 0;

struct ether_addr {
	uint8_t addr_bytes[6];
} __attribute__((__packed__));

struct ether_hdr {
	struct ether_addr dst_addr;
	struct ether_addr src_addr;
	uint16_t ether_type;
} __attribute__((__packed__));

static void
lwip_process_pkt(int len, void * pkt)
{
	void *pl;
	struct pbuf *p;
	
	pl = pkt;
	pl = (void *)(pl + sizeof(struct ether_hdr));
	p = pbuf_alloc(PBUF_IP, (len-sizeof(struct ether_hdr)), PBUF_ROM);
	assert(p);
	p->payload = pl;
	if (cos_interface.input(p, &cos_interface) != ERR_OK) {
		assert(0);
	}
	printc("lwip input done\n");

	assert(p);

	if (p->ref != 0) {
		pbuf_free(p);
	}

	return;
}

static void
process_packets(cos_portid_t port_id, char** rx_pkts, uint16_t nb_pkts)
{
	int i;

	for (i = 0; i < nb_pkts; i++) {
		char * pkt = cos_get_packet(rx_pkts[i]);
		lwip_process_pkt(100, pkt);
	}
}

static void
cos_nic_start(){
	int i;
	uint16_t nb_pkts = 0;

	char* rx_packets[MAX_PKT_BURST];

	while (1)
	{
		/* infinite loop to process packets */
		for (i = 0; i < nic_ports; i++) {
			/* process rx */
			nb_pkts = cos_dev_port_rx_burst(i, 0, rx_packets, MAX_PKT_BURST);
			if (nb_pkts != 0) {
				process_packets(i, rx_packets, nb_pkts);
			}
		}
	}
}

static char* g_mp = NULL;
static void
cos_nic_init(void)
{
	int argc, ret, nb_pkts;

	#define MAX_PKT_BURST 512

	const char *mpool_name = "cos_app_pool";
	uint16_t i;
	uint16_t nb_rx_desc = NB_RX_DESC_DEFAULT;
	uint16_t nb_tx_desc = NB_TX_DESC_DEFAULT;

	/*
	 * set max_mbufs 2 times than nb_rx_desc, so that there is enough room
	 * to store packets, or this will fail if nb_rx_desc <= max_mbufs.
	 */
	const size_t max_mbufs = 8 * nb_rx_desc;

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
			"64", /* total memory used by dpdk memory subsystem, such as mempool */
			};

	argc = ARRAY_SIZE(argv);

	/* 1. do initialization of dpdk */
	ret = cos_dpdk_init(argc, argv);

	assert(ret == argc - 1); /* program name is excluded */

	/* 2. init all Ether ports */
	nic_ports = cos_eth_ports_init();

	assert(nic_ports > 0);

	/* 3. create mbuf pool where packets will be stored, user can create multiple pools */
	char* mp = cos_create_pkt_mbuf_pool(mpool_name, max_mbufs);
	g_mp = mp;

	assert(mp != NULL);

	/* 4. config each port */
	for (i = 0; i < nic_ports; i++) {
		cos_config_dev_port_queue(i, 1, 1);
		cos_dev_port_adjust_rx_tx_desc(i, &nb_rx_desc, &nb_tx_desc);
		cos_dev_port_rx_queue_setup(i, 0, nb_rx_desc, mp);
		cos_dev_port_tx_queue_setup(i, 0, nb_tx_desc);
	}

	/* 5. start each port, this will enable rx/tx */
	for (i = 0; i < nic_ports; i++) {
		cos_dev_port_start(i);
		cos_dev_port_set_promiscuous_mode(i, COS_DPDK_SWITCH_ON);
	}
}

int
nic_send_packet(char* pkt, size_t pkt_size)
{
	/* TODO: implement this interface */
	return 0;
}

int
nic_bind_port(u32_t ip_addr, u16_t port, void* share_mem, size_t share_mem_sz)
{
	/* TODO: implement this interface */
	return 0;
}

static err_t
cos_interface_output(struct netif *ni, struct pbuf *p, const ip4_addr_t *ip)
{
	void *pl;
	struct ether_hdr *eth_hdr;
	void *snd_pkt;
	int r, len;

	pl      = p->payload;
	len     = sizeof(struct ether_hdr) + p->len;
	cos_send_a_packet(p->payload, p->len, g_mp);

	assert(!r);
	return ERR_OK;
}

static err_t
cos_interface_init(struct netif *ni)
{
	ni->name[0] = 'u';
	ni->name[1] = 's';
	ni->mtu     = 1500;
	ni->output  = cos_interface_output;

	return ERR_OK;
}

void
cos_init(void)
{
	printc("nicmgr init...\n");
	cos_nic_init();

	lwip_init();
	IP4_ADDR(&ip, 10,10,1,2);
	IP4_ADDR(&mask, 255,255,255,0);
	IP4_ADDR(&gw, 10,10,1,2);

	netif_add(&cos_interface, &ip, &mask, &gw, NULL, cos_interface_init, ip4_input);
	netif_set_default(&cos_interface);
	netif_set_up(&cos_interface);
	netif_set_link_up(&cos_interface);

	printc("lwip init done\n");
}

int
main(void)
{
	printc("NIC started\n");

	cos_nic_start();

	return 0;
}
