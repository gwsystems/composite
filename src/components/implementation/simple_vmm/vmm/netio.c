#include <netio.h>
#include <string.h>
#include <netshmem.h>
#include <devices/vpci/virtio_net_io.h>


shm_bm_objid_t
netio_get_a_packet(u16_t *pkt_len)
{
	struct netshmem_pkt_buf *tx_obj;
	shm_bm_t tx_shmemd = 0;
	shm_bm_objid_t tx_pktid;
	
	tx_shmemd = netshmem_get_shm();
	tx_obj = shm_bm_alloc_net_pkt_buf(tx_shmemd, &tx_pktid);
	assert(tx_obj);

	virtio_net_send_one_pkt(tx_obj->data, pkt_len);

	return tx_pktid;
}

int
netio_send_packet(shm_bm_objid_t pktid, u16_t pkt_len)
{
	struct netshmem_pkt_buf *rx_obj;
	shm_bm_t rx_shmemd = 0;

	rx_shmemd = netshmem_get_shm();

	rx_obj = shm_bm_transfer_net_pkt_buf(rx_shmemd, pktid);
	virtio_net_rcv_one_pkt(rx_obj->data, pkt_len);
	shm_bm_free_net_pkt_buf(rx_obj);

	return 0;
}

void
netio_shmem_map(cbuf_t shm_id)
{
	netshmem_map_shmem(shm_id);
}
