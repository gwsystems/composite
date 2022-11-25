#ifndef MC_H
#define MC_H

#include <cos_component.h>
#include <shm_bm.h>

/* These proto definitions come from memcached enum network_transport, do not change it*/
#define MC_TCP_PROTO 1
#define MC_UDP_PROTO 2


void mc_map_shmem(cbuf_t shm_id);

int mc_conn_init(int proto);

u16_t mc_process_command(int fd, shm_bm_objid_t objid, u16_t data_offset, u16_t data_len);

#endif /* MC_H */
