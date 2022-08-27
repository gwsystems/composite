#ifndef INTERFACE_IMPL_H
#define INTERFACE_IMPL_H

#include <cos_types.h>
#include <cos_component.h>
#include <shm_bm.h>
#include <ck_ring.h>

#define NIC_MAX_SESSION 1024
#define NIC_MAX_SHEMEM_REGION 3

#define NIC_SHMEM_RX 0
#define NIC_SHMEM_TX 1

struct shemem_info {
	cbuf_t shmid;
	shm_bm_t shm;

	paddr_t paddr;
};

struct pkt_buf {
	char   *obj;
	char   *pkt;
	u64_t   paddr;
	int     pkt_len;
};

/* per thread session */
struct client_session {
	struct shemem_info shemem_info;

	thdid_t thd;

	u32_t ip_addr; 
	u16_t port;

	struct ck_ring *ring;
	struct pkt_buf *ringbuf;
};

extern struct ck_ring *g_tx_ring;
extern struct pkt_buf *g_tx_ringbuf;

extern struct ck_ring *g_free_ring;
extern struct pkt_buf *g_free_ringbuf;

extern struct client_session client_sessions[NIC_MAX_SESSION];

#define RX_PKT_RBUF_NUM 64
#define RX_PKT_RBUF_SZ (RX_PKT_RBUF_NUM * sizeof(struct pkt_buf))
#define RX_PKT_RING_SZ   (sizeof(struct ck_ring) + RX_PKT_RBUF_SZ)
#define RX_PKT_RING_PAGES (round_up_to_page(RX_PKT_RING_SZ)/PAGE_SIZE)

#define TX_PKT_RBUF_NUM 64
#define TX_PKT_RBUF_SZ (64 * sizeof(struct pkt_buf))
#define TX_PKT_RING_SZ   (sizeof(struct ck_ring) + TX_PKT_RBUF_SZ)
#define TX_PKT_RING_PAGES (round_up_to_page(TX_PKT_RING_SZ)/PAGE_SIZE)

#define FREE_PKT_RBUF_NUM 64
#define FREE_PKT_RBUF_SZ (64 * sizeof(struct pkt_buf))
#define FREE_PKT_RING_SZ   (sizeof(struct ck_ring) + FREE_PKT_RBUF_SZ)
#define FREE_PKT_RING_PAGES (round_up_to_page(FREE_PKT_RING_SZ)/PAGE_SIZE)

void pkt_ring_buf_init(struct ck_ring **ring, struct pkt_buf **ringbuf, size_t ringbuf_num, size_t ringbuf_sz);

int pkt_ring_buf_enqueue(struct ck_ring *ring, struct pkt_buf *ringbuf, struct pkt_buf *buf);
int pkt_ring_buf_dequeue(struct ck_ring *ring, struct pkt_buf *ringbuf, struct pkt_buf *buf);
int pkt_ring_buf_empty(struct ck_ring *tx_ring);

#endif /* INTERFACE_IMPL_H */
