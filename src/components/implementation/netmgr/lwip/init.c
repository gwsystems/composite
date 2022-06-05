
#include <cos_types.h>
#include <nicshmem.h>
#include <shm_bm.h>
#include <string.h>
#include <nic.h>

#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/tcp.h>
#include <lwip/stats.h>
#include <lwip/prot/tcp.h>

static struct ip4_addr ip, mask, gw;
static struct netif cos_interface;

struct ether_addr {
	uint8_t addr_bytes[6];
} __attribute__((__packed__));

struct ether_hdr {
	struct ether_addr dst_addr;
	struct ether_addr src_addr;
	uint16_t ether_type;
} __attribute__((__packed__));

static void
cos_interface_input(void * pkt, int len)
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
	// printc("lwip input done\n");

	assert(p);

	if (p->ref != 0) {
		pbuf_free(p);
	}

	return;
}

int
net_receive_packet(char* pkt, size_t pkt_len)
{
	// cos_interface_input(pkt, pkt_len);
	printc("net receive a packet\n");
	return 0;
}

static err_t
cos_interface_output(struct netif *ni, struct pbuf *p, const ip4_addr_t *ip)
{
	nic_send_packet(p->payload, p->len);

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

shm_bm_t shm;
shm_bm_objid_t  objid;
struct pkt_data_buf *obj;
void
cos_init(void)
{
	printc("netmgr init...\n");

	lwip_init();
	IP4_ADDR(&ip, 10,10,1,2);
	IP4_ADDR(&mask, 255,255,255,0);
	IP4_ADDR(&gw, 10,10,1,2);

	netif_add(&cos_interface, &ip, &mask, &gw, NULL, cos_interface_init, ip4_input);
	netif_set_default(&cos_interface);
	netif_set_up(&cos_interface);
	netif_set_link_up(&cos_interface);

	printc("lwip init done\n");

	void  *mem;
	cbuf_t id;
	size_t shmsz;

	shmsz = round_up_to_page(shm_bm_size_shemem_data_buf());
	id = memmgr_shared_page_allocn_aligned(shmsz/PAGE_SIZE, SHM_BM_ALIGN, (vaddr_t *)&mem);

	shm = shm_bm_create_shemem_data_buf(mem, shmsz);
	if (!shm) {
		printc("FAILURE: could not create shm from allocated memory\n");
		return;
	}
	shm_bm_init_shemem_data_buf(shm);
	nicshmem_test_map(id);
	obj = shm_bm_alloc_shemem_data_buf(shm, &objid);
	obj->flag = 0;

	nicshmem_test_objread(objid, 0);
}

int
main(void)
{
	printc("netmgr main started\n");

	while (1)
	{
		/* code */
		while (obj->flag == 0);
		cos_interface_input(obj->data, obj->data_len);

	}

	return 0;
}
