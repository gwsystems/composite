#include<llprint.h>
#include<pci.h>
#include<io.h>
#include<cos_kernel_api.h>

#include<rte_bus_pci.h>
#include <rte_mempool.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_ethdev.h>
#include <rte_ether.h>

#include "cos_dpdk_adapter.h"

COS_DECLARE_MODULE(e1000);
COS_DECLARE_MODULE(mempool_ring)

#define RX_RING_SIZE    32
#define TX_RING_SIZE    128
#define BURST_SIZE 32
#define NUM_MBUFS (1024)
#define MBUF_CACHE_SIZE 512
#define BURST_SIZE 32
#define IP_PROTOCOL_TCP 6
#define IP_PROTOCOL_UDP 17
#define RX_RING_SIZE    512
#define TX_RING_SIZE    256
#define RX_MBUF_DATA_SIZE 1024
#define RX_MBUF_SIZE (RX_MBUF_DATA_SIZE + RTE_PKTMBUF_HEADROOM + sizeof(struct rte_mbuf))

typedef uint16_t portid_t;

struct rte_port *ports;
portid_t ports_ids[32];
char name[16];
struct rte_mempool *mbuf_pool;
struct rte_mempool *rx_mbuf_pool;


extern struct rte_pci_bus rte_pci_bus;

struct pci_dev cos_pci_devices[PCI_DEVICE_MAX];
int pci_dev_nb = 0;

struct rte_mempool *tx_mbuf_pool;

static const struct rte_eth_conf port_conf_default = {
};
int
cos_printc(const char *fmt, va_list ap)
{
	char    s[128];
	size_t  ret, len = 128;

	ret = vsnprintf(s, len, fmt, ap);
	cos_llprint(s, ret);

	return ret;
}

int
cos_printf(const char *fmt,...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = cos_printc(fmt, ap);
	va_end(ap);
	return ret;
}

int
cos_bus_scan(void)
{
	int ret;

	pci_dev_nb	= pci_dev_count();
	ret		= pci_scan(cos_pci_devices, pci_dev_nb);

	pci_dev_print(cos_pci_devices, pci_dev_nb);

	return ret;
}

/* Write PCI config space. */
int
cos_pci_write_config(const struct rte_pci_device *device,
		const void *buf, size_t len, off_t offset)
{
	uint32_t buffer = 0, w = 0, w_once;
	int _len = (int)len;
	(uint32_t)offset;

	if (!buf) return -1;

	do
	{
		buffer = pci_config_read(device->addr.bus, device->addr.devid,
				device->addr.function, offset);

		w_once = _len > COS_PCI_IO_SIZE ? COS_PCI_IO_SIZE:_len;
		memcpy(&buffer, buf, w_once);

		pci_config_write(device->addr.bus, device->addr.devid,
				device->addr.function, offset, buffer);

		w	+= w_once;
		offset	+= COS_PCI_IO_SIZE;
		_len	-= COS_PCI_IO_SIZE;
		buf	+= w_once;
	} while (_len > 0);

	return w;
}

int
cos_pci_read_config(const struct rte_pci_device *device,
		void *buf, size_t len, off_t offset)
{
	uint32_t buffer, r = 0, r_once;
	int _len = (int)len;

	if (!buf) return -1;

	do
	{
		buffer = pci_config_read(device->addr.bus, device->addr.devid,
				device->addr.function, (uint32_t)offset);
		r_once = _len > COS_PCI_IO_SIZE ? COS_PCI_IO_SIZE:_len;
		memcpy(buf, &buffer, r_once);

		r +=	r_once;
		offset	+= COS_PCI_IO_SIZE;
		_len	-= COS_PCI_IO_SIZE;
	} while (_len > 0);
	
	return r;
}

int cos_pci_scan(void)
{
	int i, j;
	struct rte_pci_device *pci_device_list, *rte_dev;
	struct pci_dev *cos_dev;

	cos_bus_scan();

	pci_device_list = malloc(sizeof(*rte_dev) * pci_dev_nb);

	if (!pci_device_list) return -1;

	memset(pci_device_list, 0, sizeof(*rte_dev) * pci_dev_nb);

	cos_printf("cos pci sanning\n");

	for (i = 0; i < pci_dev_nb; i++) {
		rte_dev = &pci_device_list[i];
		cos_dev = &cos_pci_devices[i];
		/* rte_dev->device = NULL; */
		rte_dev->addr.bus = cos_dev->bus;
		rte_dev->addr.devid = cos_dev->dev;
		rte_dev->addr.function = cos_dev->func;
		rte_dev->id.class_id = cos_dev->classcode;
		rte_dev->id.vendor_id = cos_dev->vendor;
		rte_dev->id.device_id = cos_dev->device;
		rte_dev->id.subsystem_vendor_id = PCI_ANY_ID;
		rte_dev->id.subsystem_device_id = PCI_ANY_ID;
		for (j = 0; j < PCI_MAX_RESOURCE; j++) {
			rte_dev->mem_resource[j].phys_addr = cos_dev->bar[j].raw & 0xFFFFFFF0;
			if (!cos_dev->bar[j].raw) continue;

			uint32_t buf = 0;
			uint8_t offset;
			buf = 0xFFFFFFFF;
			offset = (j + 4) << 2;
			cos_pci_write_config(rte_dev, &buf, sizeof(buf), offset);
			cos_pci_read_config(rte_dev, &buf, sizeof(buf), offset);
			buf = ~(buf & ~0xF) + 1;
			rte_dev->mem_resource[j].len = buf;
			buf = cos_dev->bar[j].raw;
			cos_pci_write_config(rte_dev, &buf, sizeof(buf), offset);
			rte_dev->mem_resource[j].addr = NULL; /* Has yet to be mapped */
		}
		rte_dev->max_vfs = 0;
		rte_dev->kdrv = RTE_PCI_KDRV_UIO_GENERIC;
		pci_name_set(rte_dev);
		rte_pci_add_device(rte_dev);
	}

	return 0;
}

cos_vaddr_t
cos_map_phys_to_virt(paddr_t paddr, size_t size)
{
	return (cos_vaddr_t)cos_hw_map(cos_compinfo_get(cos_defcompinfo_curr_get()), BOOT_CAPTBL_SELF_INITHW_BASE, paddr, size);
}

cos_paddr_t
cos_map_virt_to_phys(cos_vaddr_t addr)
{
	unsigned long ret;
	cos_vaddr_t vaddr = addr;

	assert((vaddr & 0xfff) == 0);

	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	ret = call_cap_op(ci->pgtbl_cap, CAPTBL_OP_INTROSPECT, (vaddr_t)vaddr, 0, 0, 0);

	return ret & 0x00007ffffffff000;
}



void
print_ether_addr(struct rte_mbuf *m)
{
	struct rte_ether_hdr *eth_hdr;
	char buf[RTE_ETHER_ADDR_FMT_SIZE];

	eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	unsigned char *c = (char*)&(eth_hdr->ether_type);
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, &eth_hdr->src_addr);
	printc("src MAX: %s ", buf);
	rte_ether_format_addr(buf, RTE_ETHER_ADDR_FMT_SIZE, &eth_hdr->dst_addr);
	printc("dst MAC: %s\n" ,buf);
	printc("ether type:%02X%02X\n",*c, *(c+1));
}

static int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;


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
	struct rte_ether_addr addr;
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


static void
process_batch(int port, struct rte_mbuf **mbufs, int cnt)
{
	struct rte_mbuf *tx_mbufs[32];
	int i;

	if (rte_pktmbuf_alloc_bulk(tx_mbuf_pool, tx_mbufs, cnt)) {
		assert(0);
	}
	/* cnt = 0; */
	for(i=0; i<cnt; i++) {
		tx_mbufs[i]->buf_addr     = mbufs[i]->buf_addr;
		tx_mbufs[i]->buf_iova = mbufs[i]->buf_iova;
		tx_mbufs[i]->data_len = mbufs[i]->data_len; 
	}

	rte_eth_tx_burst(port, 0, tx_mbufs, cnt);
}

static void
pkt_burst_test(void)
{
	struct rte_mbuf *mbufs[BURST_SIZE];
	u8_t port;
	int tot_rx = 0, tot_tx = 0;
	

	while (1) {
		for(port=0; port<1; port++) {
			struct rte_eth_stats stats;
			u16_t nb_rx = rte_eth_rx_burst(port, 0, mbufs, BURST_SIZE);
			rte_eth_stats_get(0,&stats);
			cos_printf("stats:%d,bytes:%d, missed:%d, err1:%d, err2:%d,tx:%d\n",stats.ipackets,stats.ibytes,stats.imissed, stats.ierrors, stats.rx_nombuf,stats.opackets);
			tot_rx += nb_rx;

			if (nb_rx) {
				cos_printf("Port %d received %d packets\n", port, nb_rx);
				cos_printf("Port %d pkt_len %d data.len %d\n", mbufs[0]->port, mbufs[0]->pkt_len, mbufs[0]->data_len); 
				cos_printf("Total RX: %d \n", tot_rx);
				print_ether_addr(mbufs[0]);
				cos_printf("burst packets :%d\n",nb_rx);
				struct rte_ether_hdr *eth_hdr;
				eth_hdr = rte_pktmbuf_mtod(mbufs[0], struct rte_ether_hdr *);
				eth_hdr->src_addr.addr_bytes[0] = 1;
				eth_hdr->src_addr.addr_bytes[1] = 1;
				eth_hdr->src_addr.addr_bytes[2] = 1;
				eth_hdr->src_addr.addr_bytes[3] = 1;
				eth_hdr->src_addr.addr_bytes[4] = 1;
				eth_hdr->src_addr.addr_bytes[5] = 1;
				process_batch(port, mbufs, nb_rx);
			}	
		}
	}
	printc("going to SPIN\n");
}


int
cos_dpdk_init(int argc, char **argv)
{	
	int ret;

	rte_pci_bus.bus.scan = cos_pci_scan;

	ret = rte_eal_init(argc, argv);

	uint16_t pid = 0;
	portid_t port_id;
	uint16_t count=0;
	
	struct rte_eth_dev_info dev_info;
	struct rte_ether_addr mac_addr;
	struct rte_eth_link link;

	char link_status_text[RTE_ETH_LINK_MAX_STR_LEN];

	RTE_ETH_FOREACH_DEV(port_id) {
		ports_ids[count] = port_id;
		count++;
	}

	cos_printf("port count:%d\n",count);

	for(int i = 0; i < count; i++) {
		memset(name, 0, 16);
		rte_eth_dev_get_name_by_port(ports_ids[i], name);
		rte_eth_dev_info_get(ports_ids[i], &dev_info);
		rte_eth_macaddr_get(ports_ids[i], &mac_addr);
		cos_printf("dev_name:%s, dev_pci_name:%s, dev_mad_addr:%x:%x:%x:%x:%x:%x\n", name, dev_info.device->name, 
				mac_addr.addr_bytes[0],mac_addr.addr_bytes[1],mac_addr.addr_bytes[2],
				mac_addr.addr_bytes[3],mac_addr.addr_bytes[4],mac_addr.addr_bytes[5]);
	}

	rx_mbuf_pool = rte_pktmbuf_pool_create("RX_MBUF_POOL", NUM_MBUFS * 1, MBUF_CACHE_SIZE, 0, RX_MBUF_SIZE, -1);
	tx_mbuf_pool = rte_pktmbuf_pool_create("TX_MBUF_POOL", NUM_MBUFS * 1, MBUF_CACHE_SIZE, 0, RX_MBUF_SIZE, -1);

	printc("pktmbuf_pool created: %p, %p\n", rx_mbuf_pool, tx_mbuf_pool);
	if(!rx_mbuf_pool|| !tx_mbuf_pool) {
		assert(0);
	}

	for(int i = 0; i < count; i++) {
		ret = rte_eth_link_get(i, &link);
		if (ret < 0) {
				cos_printf("Link get failed (port %u): %s\n",
				i, rte_strerror(-ret));
			} else {
				rte_eth_link_to_str(link_status_text,
						sizeof(link_status_text),
						&link);
				cos_printf("\t%s\n", link_status_text);
		}
	}
	
	if (rte_eth_dev_cos_setup_ports(1, rx_mbuf_pool) < 0)
		return -1;

	pkt_burst_test();

	return ret;
}
