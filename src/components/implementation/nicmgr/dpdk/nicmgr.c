#include <cos_types.h>
#include <memmgr.h>
#include <netshmem.h>
#include <shm_bm.h>
#include <string.h>
#include <sched.h>
#include <nic.h>
#include <cos_dpdk.h>
#include <ck_ring.h>
#include <rte_atomic.h>
#include <sync_sem.h>
#include <sync_lock.h>
#include <arpa/inet.h>
#include "nicmgr.h"

extern volatile int debug_flag;

typedef unsigned long cos_paddr_t; /* physical address */
typedef unsigned long cos_vaddr_t; /* virtual address */

extern cos_paddr_t cos_map_virt_to_phys(cos_vaddr_t addr);
extern char *g_tx_mp[NIC_TX_QUEUE_NUM];

/* indexed by thread id */
struct client_session client_sessions[NIC_MAX_SESSION];

CK_RING_PROTOTYPE(pkt_ring_buf, pkt_buf);

struct pkt_ring_buf g_tx_ring;
struct pkt_ring_buf g_free_ring;

rte_atomic64_t tx_enqueued_miss = {0};

static char ring_buffers[NIC_MAX_SESSION][RX_PKT_RING_SZ];
void
pkt_ring_buf_init(struct pkt_ring_buf *pkt_ring_buf, size_t ringbuf_num, size_t ringbuf_sz)
{
	struct ck_ring *buf_addr;

	/* prevent multiple thread from contending memory */
	assert(cos_thdid() < NIC_MAX_SESSION);
	buf_addr = (struct ck_ring *)&ring_buffers[cos_thdid()];

	ck_ring_init(buf_addr, ringbuf_num);

	pkt_ring_buf->ring    = buf_addr;
	pkt_ring_buf->ringbuf = (struct pkt_buf *)((char *)buf_addr + sizeof(struct ck_ring));
}

inline int
pkt_ring_buf_enqueue(struct pkt_ring_buf *pkt_ring_buf, struct pkt_buf *buf)
{
	assert(pkt_ring_buf->ring && pkt_ring_buf->ringbuf);

	return CK_RING_ENQUEUE_SPSC(pkt_ring_buf, pkt_ring_buf->ring, pkt_ring_buf->ringbuf, buf);
}

inline int
pkt_ring_buf_dequeue(struct pkt_ring_buf *pkt_ring_buf, struct pkt_buf *buf)
{
	assert(pkt_ring_buf->ring && pkt_ring_buf->ringbuf);

	return CK_RING_DEQUEUE_SPSC(pkt_ring_buf, pkt_ring_buf->ring, pkt_ring_buf->ringbuf, buf);
}

inline int
pkt_ring_buf_empty(struct pkt_ring_buf *pkt_ring_buf)
{
	assert(pkt_ring_buf->ring);

	return (!ck_ring_size(pkt_ring_buf->ring));
}

shm_bm_objid_t
nic_get_a_packet(u16_t *pkt_len)
{
	thdid_t                    thd;	
	struct pkt_buf             buf;
	shm_bm_objid_t             objid;
	struct client_session     *session;
	struct netshmem_pkt_buf   *obj;
	int len;

	thd = cos_thdid();
	assert(thd < NIC_MAX_SESSION);

	session = &client_sessions[thd];

	// if (unlikely(debug_flag)) {
	// 	printc("tenant %u(%u) is to dequeue\n", ntohs(session->port), thd);
	// }
	session->blocked_loops_begin++;
	
	sync_sem_take(&session->sem);
	session->blocked_loops_end++;

	assert(!pkt_ring_buf_empty(&session->pkt_ring_buf));

	while (!pkt_ring_buf_dequeue(&session->pkt_ring_buf, &buf))
	assert(buf.pkt);

	char *pkt = cos_get_packet(buf.pkt, &len);
	assert(len < PKT_BUF_SIZE);

	obj = shm_bm_alloc_net_pkt_buf(session->shemem_info.shm, &objid);
	assert(obj);

	memcpy(obj->data, pkt, len);

#if USE_CK_RING_FREE_MBUF
	while (!pkt_ring_buf_enqueue(&g_free_ring, &buf));
#else
	cos_free_packet(buf.pkt);
#endif

	*pkt_len = len;

	return objid;
}

static void
ext_buf_free_callback_fn(void *addr, void *opaque)
{
	/* Shared mem uses borrow api, thus do not need to free it here */
	if (addr != NULL) {
		shm_bm_free_net_pkt_buf(addr);
	} else {
		printc("External buffer address is invalid\n");
		assert(0);
	}
}

extern struct sync_lock tx_lock[NUM_CPU];

int
nic_send_packet(shm_bm_objid_t pktid, u16_t pkt_offset, u16_t pkt_len)
{
	thdid_t                  thd;
	shm_bm_objid_t           objid;
	struct pkt_buf           buf;
	struct netshmem_pkt_buf *obj;

	thd   = cos_thdid();
	objid = pktid;

	obj = (struct netshmem_pkt_buf *)shm_bm_borrow_net_pkt_buf(client_sessions[thd].shemem_info.shm, objid);

	buf.obj = (char *)obj;
	buf.pkt = pkt_offset + obj->data;

	u64_t data_paddr = client_sessions[thd].shemem_info.paddr 
		+ (u64_t)buf.obj - (u64_t)client_sessions[thd].shemem_info.shm;
	
	buf.paddr   = data_paddr;
	buf.pkt_len = pkt_len;
#if 0
	if (!pkt_ring_buf_enqueue(&client_sessions[thd].pkt_tx_ring, &buf)) {
		/* tx queue is full, drop the packet */
		rte_atomic64_add(&tx_enqueued_miss, 1);
		shm_bm_free_net_pkt_buf(obj);
	}
#else 
	char *mbuf;
	void *ext_shinfo;
	char *tx_packets[256];

	coreid_t core_id = cos_cpuid();
#if E810_NIC == 0
	core_id = 1;
#endif
	mbuf = cos_allocate_mbuf(g_tx_mp[core_id - 1]);
	assert(mbuf);
	ext_shinfo = netshmem_get_tailroom((struct netshmem_pkt_buf *)buf.obj);
	cos_attach_external_mbuf(mbuf, buf.obj, buf.paddr, PKT_BUF_SIZE, ext_buf_free_callback_fn, ext_shinfo);
	cos_set_external_packet(mbuf, (buf.pkt - buf.obj), buf.pkt_len, 1);
	tx_packets[0] = mbuf;

	sync_lock_take(&tx_lock[core_id - 1]);
	cos_dev_port_tx_burst(0, core_id - 1, tx_packets, 1);
	sync_lock_release(&tx_lock[core_id - 1]);
#endif

	return 0;
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

	shm_bm_t    shm;
	cbuf_t      shmid;
	cos_paddr_t paddr = 0;
	thdid_t     thd;

	thd = cos_thdid();
	assert(thd < NIC_MAX_SESSION);

	client_sessions[thd].ip_addr = ip_addr;
	client_sessions[thd].port    = port;
	client_sessions[thd].thd     = thd;

	shm   = netshmem_get_shm();
	assert(shm);
	shmid = netshmem_get_shm_id();
	paddr = cos_map_virt_to_phys((cos_vaddr_t)shm);
	assert(paddr);

	client_sessions[thd].shemem_info.shmid = shmid;
	client_sessions[thd].shemem_info.shm   = shm;
	client_sessions[thd].shemem_info.paddr = paddr;
	cos_hash_add(client_sessions[thd].port, &client_sessions[thd]);

	sync_sem_init(&client_sessions[thd].sem, 0);

	pkt_ring_buf_init(&client_sessions[thd].pkt_ring_buf, RX_PKT_RBUF_NUM, RX_PKT_RING_SZ);
	// pkt_ring_buf_init(&client_sessions[thd].pkt_tx_ring, TX_PKT_RBUF_NUM, TX_PKT_RING_SZ);

	client_sessions[thd].blocked_loops_begin = 0;
	client_sessions[thd].blocked_loops_end = 0;
	client_sessions[thd].tx_init_done = 1;

	return 0;
}

u64_t
nic_get_port_mac_address(u16_t port)
{
	return cos_get_port_mac_address(port);
}
