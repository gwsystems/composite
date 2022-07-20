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
struct netshmem_pkt_buf *obj;

/* indexed by thread id */
struct client_session client_sessions[NIC_MAX_SESSION];

CK_RING_PROTOTYPE(pkt_ring_buf, pkt_buf);

int
pkt_ring_buf_enqueue(struct client_session *session, struct pkt_buf *buf)
{

	if (!session->ring) return -1;
	assert(session->ringbuf);

	if (ck_ring_enqueue_spsc_pkt_ring_buf(session->ring, session->ringbuf, buf) == false) return -1;

	return 0;
}
static int bb = 0;
int
pkt_ring_buf_dequeue(struct client_session *session, struct pkt_buf *buf)
{
	assert(buf);
	if (!session->ring || !session->ringbuf) return 0;

	if (ck_ring_dequeue_spsc_pkt_ring_buf(session->ring, session->ringbuf, buf) == true) return 1;

	return 0;
}

int
pkt_ring_buf_empty(struct client_session *session)
{
	struct ck_ring *cring = session->ring;

	if (!cring) return 1;

	return (!ck_ring_size(cring));
}

shm_bm_objid_t
nic_get_a_packet()
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
	while (pkt_ring_buf_empty(session)) {
		sched_thd_block(0);
	}

	pkt_ring_buf_dequeue(session, &buf);

	obj = shm_bm_alloc_rx_pkt_buf(session->shemem_info[NIC_SHMEM_RX].shm, &objid);
	char * pkt = cos_get_packet(buf.pkt, &len);
	assert(len < PKT_BUF_SIZE);

	for (i = 0; i < len; i++) {
		obj->data[i] = pkt[i];
	}

	obj->payload_offset = 0;
	obj->payload_sz = len;
	return objid;
}

static void
ext_buf_free_callback_fn(void *addr, void *opaque)
{
	bool *freed = opaque;

	if (addr == NULL) {
		printc("External buffer address is invalid\n");
		return;
	}

	*freed = true;
	printc("External buffer freed via callback\n");
}

int
nic_send_packet(shm_bm_objid_t pktid, u16_t pkt_len)
{
	thdid_t thd;
	shm_bm_objid_t   objid;
	thd = cos_thdid();
	struct netshmem_pkt_buf *obj;
	char* data;

	objid = pktid;

	obj = (struct netshmem_pkt_buf*)shm_bm_take_tx_pkt_buf(client_sessions[thd].shemem_info[NIC_SHMEM_TX].shm, objid);
	char* mbuf = cos_allocate_mbuf(g_tx_mp);

	data = obj->payload_offset + obj->data;

	u64_t data_paddr = client_sessions[thd].shemem_info[NIC_SHMEM_TX].paddr 
		+ (u64_t)data - (u64_t)client_sessions[thd].shemem_info[NIC_SHMEM_TX].shm;
	

	cos_attach_external_mbuf(mbuf, data, PKT_BUF_SIZE, ext_buf_free_callback_fn, data_paddr);

	cos_send_external_packet(mbuf, pkt_len);

	// cos_get_port_stats(0);

	return 0;
}

static void
ringbuf_init(struct client_session *session)
{
	vaddr_t buf_addr = NULL;

	buf_addr = malloc(PKT_RING_SZ);
	assert(buf_addr);

	ck_ring_init(buf_addr, PKT_RBUF_SZ);

	session->ring    = buf_addr;
	session->ringbuf = buf_addr + sizeof(struct ck_ring);
}

// extern void tls_init();
int
nic_shemem_map(cbuf_t shmid) {
	return 0;
}

void
nic_shmem_init(cbuf_t rx_shm_id, cbuf_t tx_shm_id)
{
	netshmem_map_shmem(rx_shm_id, tx_shm_id);
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

	// obj = (struct netshmem_pkt_buf*)shm_bm_take_rx_pkt_buf(netshmem_get_rx_shm(), 0);
	// printc("dpdk shmem data is: %c\n", obj->data[0]);
	shm = netshmem_get_rx_shm();
	shmid = netshmem_get_rx_shm_id();
	paddr = cos_map_virt_to_phys((cos_vaddr_t)shm);
	assert(paddr);

	client_sessions[thd].shemem_info[NIC_SHMEM_RX].shmid = shmid;
	client_sessions[thd].shemem_info[NIC_SHMEM_RX].shm   = shm;
	client_sessions[thd].shemem_info[NIC_SHMEM_RX].paddr = paddr;

	shm = netshmem_get_tx_shm();
	shmid = netshmem_get_tx_shm_id();
	paddr = cos_map_virt_to_phys((cos_vaddr_t)shm);
	assert(paddr);

	client_sessions[thd].shemem_info[NIC_SHMEM_TX].shmid = shmid;
	client_sessions[thd].shemem_info[NIC_SHMEM_TX].shm   = shm;
	client_sessions[thd].shemem_info[NIC_SHMEM_TX].paddr = paddr;
	printc("dpdk init shmem done\n");

	tls_init();
	ringbuf_init(&client_sessions[thd]);
	return 0;
}
