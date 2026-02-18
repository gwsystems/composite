#include <vmm_netio_rx.h>
#include <vmm_netio_tx.h>
#include <vmm_netio_shmem.h>
#include <string.h>
#include <netshmem.h>
#include <nic_netio_rx.h>
#include <nic_netio_tx.h>
#include <nic_netio_shmem.h>
#include <sched.h>
#include <devices/vpci/virtio_net_io.h>
#include <sync_sem.h>

shm_bm_objid_t
vmm_netio_rx_packet(u16_t *pkt_len)
{
	struct netshmem_pkt_buf *tx_obj;
	shm_bm_t tx_shmemd = 0;
	shm_bm_objid_t tx_pktid;
	u16_t _pkt_len = 0;
	
	tx_shmemd = netshmem_get_shm();
	tx_obj = shm_bm_alloc_net_pkt_buf(tx_shmemd, &tx_pktid);
	assert(tx_obj);

	virtio_net_send_one_pkt(tx_obj->data, &_pkt_len);
	*pkt_len = _pkt_len;

	return tx_pktid;
}

void pkt_hex_dump(void *_data, u16_t len)
{
    int rowsize = 16;
    int i, l, linelen, remaining;
    int li = 0;
    u8_t *data, ch; 

    data = _data;

    printc("Packet hex dump, len:%d\n", len);
    remaining = len;
    for (i = 0; i < len; i += rowsize) {
        printc("%06d\t", li);

        linelen = remaining < rowsize ? remaining : rowsize;
        remaining -= rowsize;

        for (l = 0; l < linelen; l++) {
            ch = data[l];
            printc( "%02X ", (u32_t) ch);
        }

        data += linelen;
        li += 10; 

        printc( "dump done\n");
    }
}

shm_bm_objid_t
vmm_netio_rx_packet_batch(u8_t batch_limit)
{
	struct netshmem_pkt_buf *tx_obj;
	shm_bm_t tx_shmemd = 0;
	shm_bm_objid_t tx_pktid;
	u16_t _pkt_len = 0;
	shm_bm_objid_t             first_objid;
	struct netshmem_pkt_buf   *first_obj;
	struct netshmem_pkt_pri   *first_obj_pri;
	struct netshmem_meta_tuple *pkt_arr;
	u8_t batch_ct = 0;
	
	tx_shmemd = netshmem_get_shm();
	tx_obj = first_obj = shm_bm_alloc_net_pkt_buf(tx_shmemd, &tx_pktid);
	assert(tx_obj);
	first_objid = tx_pktid;
	first_obj_pri = netshmem_get_pri(tx_obj);
	pkt_arr = (struct netshmem_meta_tuple *)&first_obj_pri->pkt_arr;
	first_obj_pri->batch_len = 0;

	while (batch_ct < batch_limit) {
		assert(tx_obj);
		if (likely(batch_ct > 0)) {
			tx_obj = shm_bm_alloc_net_pkt_buf(tx_shmemd, &tx_pktid);
			assert(tx_obj);
		}

		virtio_net_send_one_pkt(netshmem_get_data_buf(tx_obj), &_pkt_len);

		// pkt_hex_dump(netshmem_get_data_buf(tx_obj), _pkt_len);
		if (likely(_pkt_len != 0)) {
			pkt_arr[batch_ct].obj_id = tx_pktid;
			pkt_arr[batch_ct].pkt_len = _pkt_len;
			batch_ct++;
		} else {
			if (batch_ct == 0) {
				sched_thd_yield();
			} else {
				shm_bm_free_net_pkt_buf(tx_obj);
				break;
			}
		}
	}
	first_obj_pri->batch_len = batch_ct;
	return first_objid;
}


int
vmm_netio_tx_packet(shm_bm_objid_t pktid, u16_t pkt_len)
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
vmm_netio_shmem_map(cbuf_t shm_id)
{
	netshmem_map_shmem(shm_id);
}