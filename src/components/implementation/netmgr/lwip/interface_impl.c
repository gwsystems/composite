#include <cos_types.h>
#include <cos_component.h>
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

#include <netmgr.h>

int return_flag = 0;
shm_bm_objid_t           g_objid;
struct pbuf *g_pbuf = NULL;

#define TCP_MAX_SERVER (16)

typedef unsigned long cos_paddr_t; /* physical address */
typedef unsigned long cos_vaddr_t; /* virtual address */

extern struct netif net_interface;

struct tcp_server
{
	struct tcp_pcb *tp;
};

struct tcp_server tcp_servers[TCP_MAX_SERVER];

static err_t
cos_lwip_tcp_sent(void *arg, struct tcp_pcb *tp, u16_t len)
{
	// printc("tcp sent\n");
	return ERR_OK;
}


static err_t
cos_lwip_tcp_recv(void *arg, struct tcp_pcb *tp, struct pbuf *p, err_t err)
{
	err_t ret_err;

	if(p != NULL) {
		// cos_echoserver(p, tp);
		return_flag = 1;
		g_pbuf = p;
		// tcp_recved(tp, p->tot_len);
	}

	return ERR_OK;

}

static void
cos_lwip_tcp_err(void *arg,  err_t err)
{
	assert(0);
	return;
}

static err_t
cos_lwip_tcp_accept(void *arg, struct tcp_pcb *tp, err_t err)
{
	err_t ret_err;
	compid_t client = (compid_t)cos_inv_token();

	tcp_arg(tp, 0);
	tcp_err(tp, cos_lwip_tcp_err);
	tcp_recv(tp, cos_lwip_tcp_recv);
	tcp_sent(tp, cos_lwip_tcp_sent);

	tcp_servers[client].tp = tp;

	tcp_nagle_disable(tp);

	ret_err = ERR_OK;
	return ret_err;
}

void netmgr_shmem_init(cbuf_t rx_shm_id, cbuf_t tx_shm_id)
{
	nic_shmem_init(rx_shm_id, tx_shm_id);
	netshmem_map_shmem(rx_shm_id, tx_shm_id);
}

int
netmgr_tcp_bind(u32_t ip_addr, u16_t port)
{

	unsigned long npages;
	void         *mem;
	shm_bm_t shm;
	struct netshmem_pkt_buf *obj;

	struct tcp_pcb *tp;
	struct ip4_addr ipa = *(struct ip4_addr*)&ip_addr;
	err_t ret;

	compid_t client = (compid_t)cos_inv_token();
	unsigned long paddr = 0;

	nic_bind_port(ip_addr, htons(port));

	tp = tcp_new();
	assert(tp != NULL);

	tcp_servers[client].tp = tp;

	ret = tcp_bind(tp, &ipa, port);

	return ret;
}

int
netmgr_tcp_listen(u8_t backlog)
{
	struct tcp_pcb *new_tp = NULL;
	compid_t client = (compid_t)cos_inv_token();

	new_tp = tcp_listen_with_backlog(tcp_servers[client].tp, backlog);
	assert(new_tp);

	tcp_servers[client].tp = new_tp;

	return ERR_OK;
}

static void
net_interface_input(void * pkt, int len)
{
	void *pl;
	struct pbuf *p;
	
	pl = pkt;

	p = pbuf_alloc(PBUF_LINK, len, PBUF_ROM);

	assert(p);

	p->payload = pl;
	if (net_interface.input(p, &net_interface) != ERR_OK) {
		assert(0);
	}

	if (p->ref != 0) {
		pbuf_free(p);
	}

	return;
}

int
net_receive_packet(char* pkt, size_t pkt_len)
{
	net_interface_input(pkt, pkt_len);
	return 0;
}

int
netmgr_tcp_accept(struct conn_addr *client_addr)
{
	shm_bm_objid_t           objid;
	struct netshmem_pkt_buf *obj;
	size_t shmsz;
	cbuf_t rx_shm_id;
	void  *mem;

	compid_t client = (compid_t)cos_inv_token();

	netif_set_link_up(&net_interface);
	tcp_accept(tcp_servers[client].tp, cos_lwip_tcp_accept);

	// should block here
	while (!return_flag)
	{
		objid = nic_get_a_packet();
		obj = shm_bm_take_rx_pkt_buf(netshmem_get_rx_shm(), objid);

		net_receive_packet(obj->payload_offset + obj->data, obj->payload_sz);
		
	}
	g_objid = objid;
	obj->payload_offset = (char*)g_pbuf->payload - obj->data;
	obj->payload_sz = g_pbuf->len;

	return 0;
}

shm_bm_objid_t
netmgr_tcp_shmem_read(void)
{
	shm_bm_objid_t           objid;
	struct netshmem_pkt_buf *obj;

	while (!return_flag)
	{
		objid = nic_get_a_packet();
		obj = shm_bm_take_rx_pkt_buf(netshmem_get_rx_shm(), objid);

		net_receive_packet(obj->payload_offset + obj->data, obj->payload_sz);
		obj->payload_offset = (char*)g_pbuf->payload - obj->data;
		obj->payload_sz = g_pbuf->len;
		g_objid = objid;
	}

	return_flag = 0;
	return g_objid;
}

int
netmgr_tcp_shmem_write(shm_bm_objid_t objid)
{
	struct netshmem_pkt_buf *obj;

	compid_t client = (compid_t)cos_inv_token();
	char *data;

	obj = shm_bm_take_tx_pkt_buf(netshmem_get_tx_shm(), objid);
	obj->objid = objid;
	data = obj->data + obj->payload_offset;

	err_t wr_err = tcp_write(tcp_servers[client].tp, data, obj->payload_sz, 0);
	assert(wr_err == ERR_OK);

	// wr_err = tcp_output(tcp_servers[client].tp);
	// assert(wr_err == ERR_OK);

	return 0;
}
