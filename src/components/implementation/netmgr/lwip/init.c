
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
	uint16_t ether_type;
} __attribute__((__packed__));



shm_bm_t shm;
shm_bm_objid_t  objid;

static err_t
cos_interface_output(struct netif *ni, struct pbuf *p, const ip4_addr_t *ip)
{
	char * data = p->payload;
	shm_bm_objid_t  objid;
	struct netshmem_pkt_buf *obj;

	if (p->type_internal & PBUF_RAM) {
		if(p->next != NULL && p->next->type_internal & PBUF_ROM) {
			/* shemem case, pbuf is chained with a header pbuf and a data pbuf that points to shemem */
			obj = (struct netshmem_pkt_buf*)(p->next->payload - NETSHMEM_HEADROOM);
			objid = obj->objid;
			data = obj->payload_offset + obj->data;
			memcpy(data - p->len, p->payload, p->len);
			obj->payload_offset = obj->payload_offset - p->len;
			obj->payload_sz += p->len;
			nic_send_packet(objid, obj->payload_sz);
		} else {
			/* other cases that don't use shmem */
			obj = shm_bm_alloc_tx_pkt_buf(netshmem_get_tx_shm(), &objid);
			if (obj == NULL) return ERR_OK;

			memcpy(obj->data, data, p->len);
			obj->payload_offset = 0;
			obj->payload_sz = p->len;

			nic_send_packet(objid, obj->payload_sz);
			shm_bm_free_tx_pkt_buf(obj);
		}
	}
	return ERR_OK;
}

static err_t
cos_interface_init(struct netif *ni)
{
	ni->name[0] = 'u';
	ni->name[1] = 's';
	ni->mtu     = 1500;
	ni->output  = etharp_output;

	ni->linkoutput = cos_interface_output;

	ni->flags |= NETIF_FLAG_ETHARP;
	ni->flags |= NETIF_FLAG_ETHERNET;

	ni->hwaddr[0] = 0x66;
	ni->hwaddr[1] = 0x66;
	ni->hwaddr[2] = 0x66;
	ni->hwaddr[3] = 0x66;
	ni->hwaddr[4] = 0x66;
	ni->hwaddr[5] = 0x66;
	ni->hwaddr_len = ETH_HWADDR_LEN;

	return ERR_OK;
}



void
cos_init(void)
{
	void  *mem;
	cbuf_t id;
	size_t shmsz;


	printc("netmgr init...\n");

	IP4_ADDR(&ip, 10,10,1,2);
	IP4_ADDR(&mask, 255,255,255,0);
	IP4_ADDR(&gw, 10,10,1,10);
	IP4_ADDR(&client, 10,10,1,2);



	lwip_init();
	netif_add(&net_interface, &ip, &mask, &gw, NULL, cos_interface_init, ethernet_input);
	netif_set_default(&net_interface);
	netif_set_up(&net_interface);
	printc("netmgr init done\n");
	// ip4_addr_t ipaddr;
	// struct eth_addr ethaddr;
	// ethaddr.addr[0] = 0x11;
	// ethaddr.addr[1] = 0x11;
	// ethaddr.addr[2] = 0x11;
	// ethaddr.addr[3] = 0x11;
	// ethaddr.addr[4] = 0x11;
	// ethaddr.addr[5] = 0x11;
	// etharp_add_static_entry(&client, &ethaddr);
}


int
main(void)
{
	printc("netmgr main started\n");

	while (1)
	{

	}

	return 0;
}
