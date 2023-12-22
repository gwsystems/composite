#ifndef NETIO_H
#define NETIO_H

#include <cos_types.h>
#include <cos_component.h>
#include <cos_stubs.h>
#include <shm_bm.h>

shm_bm_objid_t netio_get_a_packet(u16_t *pkt_len);
int netio_send_packet(shm_bm_objid_t pktid, u16_t pkt_len);
void netio_shmem_map(cbuf_t shm_id);

#endif
