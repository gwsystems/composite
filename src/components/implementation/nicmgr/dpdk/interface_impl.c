#include <cos_types.h>
#include <memmgr.h>
#include <netshmem.h>
#include <shm_bm.h>
#include <string.h>
#include <sched.h>
#include <nic.h>
#include <cos_dpdk.h>
#include <ck_ring.h>
#include "interface_impl.h"

typedef unsigned long cos_paddr_t; /* physical address */
typedef unsigned long cos_vaddr_t; /* virtual address */

extern cos_paddr_t cos_map_virt_to_phys(cos_vaddr_t addr);
extern char* g_tx_mp;
extern char* g_rx_mp;

/* indexed by thread id */
struct client_session client_sessions[NIC_MAX_SESSION];

CK_RING_PROTOTYPE(pkt_ring_buf, pkt_buf);

struct ck_ring *g_tx_ring = NULL;
struct pkt_buf *g_tx_ringbuf = NULL;

struct ck_ring *g_free_ring = NULL;
struct pkt_buf *g_free_ringbuf = NULL;

int
pkt_ring_buf_enqueue(struct ck_ring *ring, struct pkt_buf *ringbuf, struct pkt_buf *buf)
{
	assert(ring && ringbuf);

	if (ck_ring_enqueue_spsc_pkt_ring_buf(ring, ringbuf, buf) == false) return -1;

	return 0;
}

int
pkt_ring_buf_dequeue(struct ck_ring *ring, struct pkt_buf *ringbuf, struct pkt_buf *buf)
{
	assert(ring && ringbuf);

	if (ck_ring_dequeue_spsc_pkt_ring_buf(ring, ringbuf, buf) == true) return 1;

	return 0;
}

int
pkt_ring_buf_empty(struct ck_ring *ring)
{
	assert(ring);

	return (!ck_ring_size(ring));
}

shm_bm_objid_t
nic_get_a_packet(u16_t *pkt_len)
{
	thdid_t                thd;
	struct client_session *session;
	struct pkt_buf         buf;
	shm_bm_objid_t         objid;
	struct netshmem_pkt_buf    *obj;
	int i, len;

	thd = cos_thdid();
	assert(thd < NIC_MAX_SESSION);

	session = &client_sessions[thd];
	while (pkt_ring_buf_empty(session->ring)) {
		sched_thd_block(0);
	}

	pkt_ring_buf_dequeue(session->ring, session->ringbuf, &buf);
	assert(buf.pkt);

	obj = shm_bm_alloc_net_pkt_buf(session->shemem_info.shm, &objid);
	assert(obj);

	char * pkt = cos_get_packet(buf.pkt, &len);
	assert(len < PKT_BUF_SIZE);

	memcpy(obj->data, pkt, len);
	pkt_ring_buf_enqueue(g_free_ring, g_free_ringbuf, &buf);

	*pkt_len = len;

	return objid;
}

int
nic_send_packet(shm_bm_objid_t pktid, u16_t pkt_offset, u16_t pkt_len)
{
	thdid_t thd;
	shm_bm_objid_t   objid;
	struct netshmem_pkt_buf *obj;
	struct pkt_buf buf;

	thd = cos_thdid();
	objid = pktid;

	obj = (struct netshmem_pkt_buf*)shm_bm_take_net_pkt_buf(client_sessions[thd].shemem_info.shm, objid);

	buf.pkt = pkt_offset + obj->data;

	u64_t data_paddr = client_sessions[thd].shemem_info.paddr 
		+ (u64_t)buf.pkt - (u64_t)client_sessions[thd].shemem_info.shm;
	
	buf.paddr = data_paddr;
	buf.pkt_len = pkt_len;

	pkt_ring_buf_enqueue(g_tx_ring, g_tx_ringbuf, &buf);

	return 0;
}

void
pkt_ring_buf_init(struct ck_ring **ring, struct pkt_buf **ringbuf, size_t ringbuf_num, size_t ringbuf_sz)
{
	vaddr_t buf_addr = NULL;

	buf_addr = malloc(ringbuf_sz);
	assert(buf_addr);

	ck_ring_init(buf_addr, ringbuf_num);

	*ring    = buf_addr;
	*ringbuf = buf_addr + sizeof(struct ck_ring);
}

void
nic_shmem_map(cbuf_t shm_id)
{
	netshmem_map_shmem(shm_id);
}

int
nic_bind_port(u32_t ip_addr, u16_t port)
{
	unsigned long npages;
	void         *mem;
	shm_bm_t shm;
	cbuf_t shmid;
	cos_paddr_t paddr = 0;
	thdid_t thd;

	thd = cos_thdid();
	assert(thd < NIC_MAX_SESSION);

	client_sessions[thd].ip_addr = ip_addr;
	client_sessions[thd].port    = port;
	client_sessions[thd].thd     = thd;

	shm = netshmem_get_shm();
	shmid = netshmem_get_shm_id();
	paddr = cos_map_virt_to_phys((cos_vaddr_t)shm);
	assert(paddr);

	client_sessions[thd].shemem_info.shmid = shmid;
	client_sessions[thd].shemem_info.shm   = shm;
	client_sessions[thd].shemem_info.paddr = paddr;

	pkt_ring_buf_init(&client_sessions[thd].ring, &client_sessions[thd].ringbuf, RX_PKT_RBUF_NUM, RX_PKT_RING_SZ);
	return 0;
}
