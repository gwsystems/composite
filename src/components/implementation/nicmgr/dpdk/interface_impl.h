#ifndef INTERFACE_IMPL_H
#define INTERFACE_IMPL_H

#include <cos_types.h>
#include <cos_component.h>
#include <shm_bm.h>
#include <ck_ring.h>

#define NIC_MAX_SESSION 256
#define NIC_MAX_SHEMEM_REGION 3

#define NIC_SHMEM_RX 0
#define NIC_SHMEM_TX 1

struct shemem_info {
	cbuf_t shmid;
	shm_bm_t shm;

	paddr_t paddr;
};

struct pkt_buf {
	char   *pkt;
	u64_t   paddr;
	int     pkt_len;
};

/* per thread session */
struct client_session {
	struct shemem_info shemem_info[NIC_MAX_SHEMEM_REGION];

	thdid_t thd;

	u32_t ip_addr; 
	u16_t port;

	struct ck_ring *ring;
	struct pkt_buf *ringbuf;
};

extern struct ck_ring *g_tx_ring;
extern struct pkt_buf *g_tx_ringbuf;

extern struct client_session client_sessions[NIC_MAX_SESSION];

#define RX_PKT_RBUF_SZ (64 * sizeof(struct pkt_buf))
#define RX_PKT_RING_SZ   (sizeof(struct ck_ring) + RX_PKT_RBUF_SZ)
#define RX_PKT_RING_PAGES (round_up_to_page(RX_PKT_RING_SZ)/PAGE_SIZE)

#define TX_PKT_RBUF_SZ (64 * sizeof(struct pkt_buf))
#define TX_PKT_RING_SZ   (sizeof(struct ck_ring) + TX_PKT_RBUF_SZ)
#define TX_PKT_RING_PAGES (round_up_to_page(TX_PKT_RING_SZ)/PAGE_SIZE)

int rx_pkt_ring_buf_enqueue(struct client_session *session, struct pkt_buf *buf);
int rx_pkt_ring_buf_dequeue(struct client_session *session, struct pkt_buf *buf);
int rx_pkt_ring_buf_empty(struct client_session *session);

int tx_pkt_ring_buf_enqueue(struct ck_ring *tx_ring, struct pkt_buf *tx_ringbuf, struct pkt_buf *buf);
int tx_pkt_ring_buf_dequeue(struct ck_ring *tx_ring, struct pkt_buf *tx_ringbuf, struct pkt_buf *buf);
int tx_pkt_ring_buf_empty(struct ck_ring *tx_ring);

#endif /* INTERFACE_IMPL_H */
