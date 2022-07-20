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

extern struct client_session client_sessions[NIC_MAX_SESSION];

#define PKT_RBUF_SZ (64 * sizeof(struct pkt_buf))
#define PKT_RING_SZ   (sizeof(struct ck_ring) + PKT_RBUF_SZ)
#define PKT_RING_PAGES (round_up_to_page(PKT_RING_SZ)/PAGE_SIZE)

int pkt_ring_buf_enqueue(struct client_session *session, struct pkt_buf *buf);
int pkt_ring_buf_dequeue(struct client_session *session, struct pkt_buf *buf);
int pkt_ring_buf_empty(struct client_session *session);
#endif /* INTERFACE_IMPL_H */
