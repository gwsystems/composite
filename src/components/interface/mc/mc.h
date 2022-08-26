#ifndef MC_H
#define MC_H

#include <cos_component.h>
#include <shm_bm.h>

void mc_map_shmem(cbuf_t shm_id);

int mc_conn_init(void);

void mc_process_command(int fd, shm_bm_objid_t objid, u16_t data_offset, u16_t data_len);

#endif /* MC_H */
