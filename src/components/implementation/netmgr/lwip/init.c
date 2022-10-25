
#include <cos_types.h>
#include <netshmem.h>
#include <shm_bm.h>
#include <string.h>
#include <nic.h>
#include <contigmem.h>

#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/tcp.h>
#include <lwip/stats.h>
#include <lwip/prot/tcp.h>
#include <netif/ethernet.h>
#include <lwip/etharp.h>

static struct ip4_addr ip, mask, gw, client;
struct netif net_interface;

struct ether_addr {
	uint8_t addr_bytes[6];
} __attribute__((__packed__));

struct ether_hdr {
	struct ether_addr dst_addr;
	struct ether_addr src_addr;
	uint16_t          ether_type;
} __attribute__((__packed__));

static err_t
cos_interface_output(struct netif *ni, struct pbuf *p)
{
	char *data = p->payload;	
	struct netshmem_pkt_buf *obj = NULL;

	shm_bm_objid_t objid;

	u16_t pkt_offset, pkt_len;

	/* 
	 * We assume that lwip always produces either a contiguous packet, 
	 * or a packet of headers followed by a single packet with payload.
	 */
	assert(p->next == NULL || p->next->next == NULL);
	if (p->type_internal & PBUF_RAM) {
		if (p->next != NULL && p->next->type_internal & PBUF_ROM) {
			/* shemem case, pbuf is chained with a header pbuf and a data pbuf that points to shemem */
			obj   = (struct netshmem_pkt_buf*)(p->next->payload - netshmem_get_data_offset());
			objid = shm_bm_get_objid_net_pkt_buf(obj);
			data  = netshmem_get_data_buf(obj) ;

			memcpy(data - p->len, p->payload, p->len);

			pkt_offset = netshmem_get_data_offset() - p->len;

			pkt_len  = p->len;
			pkt_len += p->next->len;

			nic_send_packet(objid, pkt_offset, pkt_len);
		} else {
			/* other cases that don't use shmem */
			obj = shm_bm_alloc_net_pkt_buf(netshmem_get_shm(), &objid);
			assert(obj);

			memcpy(obj->data, data, p->len);
			pkt_offset = 0;
			pkt_len = p->len;

			nic_send_packet(objid, pkt_offset, pkt_len);
			shm_bm_free_net_pkt_buf(obj);
		}
	}
	return ERR_OK;
}

static err_t
cos_interface_init(struct netif *ni)
{
	uint64_t mac_addr = nic_get_port_mac_address(0);
	char *mac_addr_arr = (char *)&mac_addr;

	ni->name[0] = 'u';
	ni->name[1] = 's';
	ni->mtu     = 1500;
	ni->output  = etharp_output;

	ni->linkoutput = cos_interface_output;

	ni->flags |= NETIF_FLAG_ETHARP;
	ni->flags |= NETIF_FLAG_ETHERNET;

	ni->hwaddr[0] = mac_addr_arr[5];
	ni->hwaddr[1] = mac_addr_arr[4];
	ni->hwaddr[2] = mac_addr_arr[3];
	ni->hwaddr[3] = mac_addr_arr[2];
	ni->hwaddr[4] = mac_addr_arr[1];
	ni->hwaddr[5] = mac_addr_arr[0];
	ni->hwaddr_len = ETH_HWADDR_LEN;

	return ERR_OK;
}

/* 
 * This flag is needed when adding static ARP entry.
 *
 * As the LWIP netif_set_link_up() will by default send out
 * a gratuitous ARP when linking up. We don't want this behavior 
 * currently. Thus, we manually change the flag.
 */
static void
cos_netif_set_link_up(struct netif *netif)
{
	netif_set_flags(netif, NETIF_FLAG_LINK_UP);
}

void
cos_init(void)
{
	void  *mem;
	cbuf_t id;
	size_t shmsz;
	struct eth_addr ethaddr;


	printc("netmgr init...\n");

	IP4_ADDR(&ip, 10,10,1,2);
	IP4_ADDR(&mask, 255,255,255,0);
	IP4_ADDR(&gw, 10,10,1,10);

	lwip_init();
	netif_add(&net_interface, &ip, &mask, &gw, NULL, cos_interface_init, ethernet_input);
	netif_set_default(&net_interface);
	netif_set_up(&net_interface);
	cos_netif_set_link_up(&net_interface);

	/* Add static arp entry for client IP */
	IP4_ADDR(&client, 10,10,1,1);
	ethaddr.addr[0] = 0x52;
	ethaddr.addr[1] = 0x9c;
	ethaddr.addr[2] = 0xd1;
	ethaddr.addr[3] = 0x13;
	ethaddr.addr[4] = 0xa2;
	ethaddr.addr[5] = 0x0e;
	etharp_add_static_entry(&client, &ethaddr);

	printc("netmgr init done\n");
}

int main()
{
	return 0;
}