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
#include <lwip/udp.h>
#include <lwip/stats.h>
#include <lwip/prot/tcp.h>
#include <netif/ethernet.h>
#include <lwip/etharp.h>

#include <netmgr.h>

/* This is used to indicate whether the packet is a data packet to application or a state packet to lwip */
static int back_to_app = 0;

static shm_bm_objid_t g_objid;

static u16_t g_data_offset, g_data_len;

static ip_addr_t g_remote_addr;
static u16_t g_remote_port;

static struct pbuf *g_pbuf = NULL;

#define LWIP_MAX_CONNS (16)

extern struct netif net_interface;

struct lwip_pcb
{
	struct tcp_pcb *tp;
	struct udp_pcb *up;
};

static struct lwip_pcb lwip_connections[LWIP_MAX_CONNS];

static err_t
cos_lwip_tcp_sent(void *arg, struct tcp_pcb *tp, u16_t len)
{
	/* TODO: process sent state */
	return ERR_OK;
}


static err_t
cos_lwip_tcp_recv(void *arg, struct tcp_pcb *tp, struct pbuf *p, err_t err)
{
	if (p != NULL) {
		back_to_app = 1;
		g_pbuf = p;
		tcp_recved(tp, p->tot_len);
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
	thdid_t thd = cos_thdid();

	tcp_arg(tp, 0);
	tcp_err(tp, cos_lwip_tcp_err);
	tcp_recv(tp, cos_lwip_tcp_recv);
	tcp_sent(tp, cos_lwip_tcp_sent);

	lwip_connections[thd].tp = tp;

	tcp_nagle_disable(tp);

	ret_err = ERR_OK;
	return ret_err;
}

void netmgr_shmem_map(cbuf_t shm_id)
{
	nic_shmem_map(shm_id);
	netshmem_map_shmem(shm_id);
}

int
netmgr_tcp_bind(u32_t ip_addr, u16_t port)
{
	unsigned long npages;
	shm_bm_t      shm;
	err_t         ret;

	void                    *mem;
	struct netshmem_pkt_buf *obj;

	struct tcp_pcb *tp;
	struct ip4_addr ipa = *(struct ip4_addr*)&ip_addr;

	thdid_t thd = cos_thdid();

	nic_bind_port(ip_addr, htons(port));

	tp = tcp_new();
	assert(tp != NULL);

	lwip_connections[thd].tp = tp;

	ret = tcp_bind(tp, &ipa, port);

	return ret;
}

int
netmgr_tcp_listen(u8_t backlog)
{
	struct tcp_pcb *new_tp = NULL;
	thdid_t         thd    = cos_thdid();

	new_tp = tcp_listen_with_backlog(lwip_connections[thd].tp, backlog);
	assert(new_tp);

	lwip_connections[thd].tp = new_tp;

	return ERR_OK;
}

static void
net_interface_input(void *pkt, int len)
{
	void        *pl;
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
net_receive_packet(char *pkt, size_t pkt_len)
{
	net_interface_input(pkt, pkt_len);
	return 0;
}

int
netmgr_tcp_accept(struct conn_addr *client_addr)
{
	shm_bm_objid_t           objid = 0;
	struct netshmem_pkt_buf *obj;
	size_t shmsz;
	cbuf_t rx_shm_id;
	void  *mem;
	u16_t  pkt_len;

	thdid_t thd = cos_thdid();

	netif_set_link_up(&net_interface);
	tcp_accept(lwip_connections[thd].tp, cos_lwip_tcp_accept);

	while (!back_to_app) {
		objid = nic_get_a_packet(&pkt_len);
		obj   = shm_bm_take_net_pkt_buf(netshmem_get_shm(), objid);
		net_receive_packet(obj->data, pkt_len);
	}
	g_objid = objid;

	return 0;
}

shm_bm_objid_t
netmgr_tcp_shmem_read(u16_t *data_offset, u16_t *data_len)
{
	shm_bm_objid_t           objid;
	struct netshmem_pkt_buf *obj;
	u16_t pkt_len;

	obj = shm_bm_take_net_pkt_buf(netshmem_get_shm(), g_objid);

	while (!back_to_app) {
		objid = nic_get_a_packet(&pkt_len);
		obj = shm_bm_take_net_pkt_buf(netshmem_get_shm(), objid);
		assert(obj);

		net_receive_packet(obj->data, pkt_len);
		g_objid = objid;
	}

	*data_offset = (char *)g_pbuf->payload - obj->data;
	*data_len = g_pbuf->len;

	back_to_app = 0;

	return g_objid;
}

int
netmgr_tcp_shmem_write(shm_bm_objid_t objid, u16_t data_offset, u16_t data_len)
{
	struct netshmem_pkt_buf *obj;

	thdid_t thd = cos_thdid();
	char *data;

	obj = shm_bm_take_net_pkt_buf(netshmem_get_shm(), objid);
	data = obj->data + data_offset;

	err_t wr_err = tcp_write(lwip_connections[thd].tp, data, data_len, 0);
	assert(wr_err == ERR_OK);

	/* tcp_output() might be needed in the future */
	// wr_err = tcp_output(lwip_connections[thd].tp);
	// assert(wr_err == ERR_OK);

	return 0;
}

static void
cos_lwip_udp_recv(void *arg, struct udp_pcb *up, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
	if (p != NULL) {
		back_to_app = 1;
		g_pbuf = p;
		g_remote_addr = *addr;
		g_remote_port = port;
	}
}

int
netmgr_udp_bind(u32_t ip_addr, u16_t port)
{
	unsigned long npages;
	void         *mem;
	shm_bm_t      shm;

	struct netshmem_pkt_buf *obj;

	struct ip4_addr ipa = *(struct ip4_addr *)&ip_addr;
	err_t ret;

	thdid_t thd = cos_thdid();

	nic_bind_port(ip_addr, htons(port));

	lwip_connections[thd].up = udp_new();
	assert(lwip_connections[thd].up != NULL);

	ret = udp_bind(lwip_connections[thd].up, &ipa, port);
	assert(ret == ERR_OK);

	udp_recv(lwip_connections[thd].up, cos_lwip_udp_recv, 0);

	return ret;
}

shm_bm_objid_t
netmgr_udp_shmem_read(u16_t *data_offset, u16_t *data_len, u32_t *remote_addr, u16_t *remote_port)
{
	shm_bm_objid_t           objid;
	struct netshmem_pkt_buf *obj;
	u16_t pkt_len;

	while (1) {
		objid = nic_get_a_packet(&pkt_len);
		obj = shm_bm_borrow_net_pkt_buf(netshmem_get_shm(), objid);
		assert(obj);

		net_receive_packet(obj->data, pkt_len);
		g_objid = objid;
		if (!back_to_app) {
			/* if this packet doesn't need to transfer to application, lwip needs to free it. */
			shm_bm_free_net_pkt_buf(obj);
		} else {
			break;
		}
	}

	*data_offset = (char *)g_pbuf->payload - obj->data;
	*data_len    = g_pbuf->len;
	*remote_addr = g_remote_addr.addr;
	*remote_port = g_remote_port;

	back_to_app = 0;
	return g_objid;
}

int
netmgr_udp_shmem_write(shm_bm_objid_t objid, u16_t data_offset, u16_t data_len, u32_t remote_ip, u16_t remote_port)
{
	struct netshmem_pkt_buf *obj;
	struct pbuf             *p;

	ip_addr_t dst_ip;
	
	thdid_t thd = cos_thdid();
	char *data;

	obj  = shm_bm_borrow_net_pkt_buf(netshmem_get_shm(), objid);
	data = obj->data + data_offset;

	p = pbuf_alloc(PBUF_LINK, data_len, PBUF_ROM);
	assert(p);

	p->payload  = data;
	dst_ip.addr = remote_ip;

	udp_sendto_if(lwip_connections[thd].up, p , &dst_ip, remote_port, &net_interface);
	pbuf_free(p);

	return 0;
}
