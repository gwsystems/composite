#pragma once

#include <nf_session.h>

CK_RING_PROTOTYPE(nf_tx_ring_buf, nf_pkt_meta_data);

static char ring_buffers[NF_TX_MAX_RING_NUM][NF_TX_PKT_RING_SZ];
static struct nf_session nf_sessions[VMRT_MAX_VM][MAX_SVC_NUM];

void
nf_sessions_init(void)
{
	for (int i = 0; i < VMRT_MAX_VM; i++) {
		for (int j = 0; j < MAX_SVC_NUM; j++) {
			nf_sessions[i][j].rx_shmemd = 0;
			nf_sessions[i][j].rx_thd = 0;
			nf_sessions[i][j].tx_shmemd = 0;
			nf_sessions[i][j].tx_thd = 0;
			nf_sessions[i][j].nf_tx_ring_buf.ring = 0;
			nf_sessions[i][j].nf_tx_ring_buf.ringbuf = 0;

		}
	}
}

void
nf_tx_ring_buf_init(struct nf_tx_ring_buf *pkt_ring_buf, size_t ringbuf_num, size_t ringbuf_sz)
{
	struct ck_ring *buf_addr;

	/* prevent multiple thread from contending memory */
	assert(cos_thdid() < 30);
	buf_addr = (struct ck_ring *)&ring_buffers[cos_thdid()];

	ck_ring_init(buf_addr, ringbuf_num);

	pkt_ring_buf->ring    = buf_addr;
	pkt_ring_buf->ringbuf = (struct pkt_buf *)((char *)buf_addr + sizeof(struct ck_ring));
}

inline int
nf_tx_ring_buf_enqueue(struct nf_tx_ring_buf *pkt_ring_buf, struct nf_pkt_meta_data *buf)
{
	assert(pkt_ring_buf->ring && pkt_ring_buf->ringbuf);

	return CK_RING_ENQUEUE_SPSC(nf_tx_ring_buf, pkt_ring_buf->ring, pkt_ring_buf->ringbuf, buf);
}

inline int
nf_tx_ring_buf_dequeue(struct nf_tx_ring_buf *pkt_ring_buf, struct nf_pkt_meta_data *buf)
{
	assert(pkt_ring_buf->ring && pkt_ring_buf->ringbuf);

	return CK_RING_DEQUEUE_SPSC(nf_tx_ring_buf, pkt_ring_buf->ring, pkt_ring_buf->ringbuf, buf);
}

inline int
nf_tx_ring_buf_empty(struct nf_tx_ring_buf *pkt_ring_buf)
{
	assert(pkt_ring_buf->ring);

	return (!ck_ring_size(pkt_ring_buf->ring));
}

struct nf_session *get_nf_session(struct vmrt_vm_comp *vm, int svc_id)
{
	if (svc_id < 0) return NULL;
	else return &nf_sessions[vm->comp_id][svc_id];
}

void
nf_session_tx_update(struct nf_session *session, shm_bm_t tx_shmemd, thdid_t tx_thd)
{
	session->tx_shmemd = tx_shmemd;
	session->tx_thd = tx_thd;
}
