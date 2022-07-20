#ifndef NETSHMEM_H
#define NETSHMEM_H

#include <cos_component.h>
#include <shm_bm.h>

#define PKT_BUF_NUM 4096
#define PKT_BUF_SIZE 2000
#define PKT_BUF_HEAD_ROOM 500

struct netshmem {
	size_t shmsz;
	shm_bm_t shm;
	cbuf_t shm_id;
};

#define NETSHMEM_RX 0
#define NETSHMEM_TX 1
#define NETSHMEM_REGION_SZ 2

/*
 * This is the offset size of netshmem_pkt_buf.data for application to write data.
 * HEADROOM is used for lwip to set ether & ip & tcp/udp headers
 */

#define NETSHMEM_HEADROOM 128

struct netshmem_pkt_buf {
	char data[PKT_BUF_SIZE];
	u32_t payload_offset; /* offset of payload data within data member */
	int payload_sz;
	shm_bm_objid_t objid; /* self reference */
};

SHM_BM_INTERFACE_CREATE(rx_pkt_buf, sizeof (struct netshmem_pkt_buf), PKT_BUF_NUM);
SHM_BM_INTERFACE_CREATE(tx_pkt_buf, sizeof (struct netshmem_pkt_buf), PKT_BUF_NUM);

void netshmem_init(void);
cbuf_t netshmem_get_rx_shm_id(void);
cbuf_t netshmem_get_tx_shm_id(void);

shm_bm_t netshmem_get_rx_shm(void);
shm_bm_t netshmem_get_tx_shm(void);

void netshmem_map_shmem(cbuf_t rx_shm_id, cbuf_t tx_shm_id);

#endif
