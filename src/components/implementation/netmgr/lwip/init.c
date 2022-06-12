
#include <cos_types.h>
#include <nicshmem.h>
#include <shm_bm.h>
#include <string.h>
#include <nic.h>
#include <contigmem.h>

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
	// nic_send_packet(p->payload, p->len);

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
struct data_buffer *obj;

void
cos_init(void)
{
	void  *mem;
	cbuf_t id;
	size_t shmsz;


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

	nic_bind_port(ip.addr, 80);
	shmsz = round_up_to_page(shm_bm_size_data_buf());
	id = contigmem_shared_alloc_aligned(shmsz/PAGE_SIZE, SHM_BM_ALIGN, (vaddr_t *)&mem);

	shm = shm_bm_create_data_buf(mem, shmsz);

	if (!shm) {
		printc("FAILURE: could not create shm from allocated memory\n");
		return;
	}
	shm_bm_init_data_buf(shm);
	nic_shemem_map(id);
	obj = shm_bm_alloc_data_buf(shm, &objid);

	obj->data[0] = 0x66;
	obj->data[1] = 0x66;
	obj->data[2] = 0x66;
	obj->data[3] = 0x66;
	obj->data[4] = 0x66;
	obj->data[5] = 0x66;
	obj->data[6] = 0xF0;
	obj->data[7] = 0xB6;
	obj->data[8] = 0x1E;
	obj->data[9] = 0x94;
	obj->data[10] = 0x93;
	obj->data[11] = 0x5F;
	obj->data[12] = 0x08;
	obj->data[13] = 0x00;
	obj->data[14] = 0x45;
	obj->data[15] = 0x00;
	obj->data[16] = 0x00;
	obj->data[17] =0x1C;
	obj->data[18] =0x00;
	obj->data[19] =0x01;
	obj->data[20] =0x00;
	obj->data[21] =0x00;
	obj->data[22] =0x40;
	obj->data[23] =0x01;
	obj->data[24] =0x64;
	obj->data[25] =0xCA;
	obj->data[26] =0x0A;
	obj->data[27] =0x0A;
	obj->data[28] =0x01;
	obj->data[29] =0x01;
	obj->data[30] =0x0A;
	obj->data[31] =0x0A;
	obj->data[32] =0x01;
	obj->data[33] =0x02;
	obj->data[34] =0x08;
	obj->data[35] =0x00;
	obj->data[36] =0xF7;
	obj->data[37] =0xFF;
	obj->data[38] =0x00;
	obj->data[39] =0x00;
	obj->data[40] =0x00;
	obj->data[41] =0x00;
	printc("prepare send a pkt\n");
	
		/* code */
	nic_send_packet(objid, 200);
}

int
main(void)
{
	// printc("netmgr main started\n");

	// nic_get_a_packet(0);
	while (1)
	{
		// printc("hello from lwip\n");
		/* code */
		// while (obj->flag == 0);
		// cos_interface_input(obj->data, obj->data_len);

	}

	return 0;
}
