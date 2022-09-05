#ifndef NETMGR_H
#define NETMGR_H

#include <cos_types.h>
#include <cos_component.h>
#include <cos_stubs.h>
#include <shm_bm.h>

#define NETMGR_OK (0)

struct conn_addr {
	u32_t ip;
	u16_t port;
};

void netmgr_shmem_map(cbuf_t shm_id);

int netmgr_tcp_bind(u32_t ip_addr, u16_t port);

int netmgr_tcp_listen(u8_t backlog);
int netmgr_tcp_accept(struct conn_addr *client_addr);

shm_bm_objid_t netmgr_tcp_shmem_read(u16_t *data_offset, u16_t *data_len);
int netmgr_tcp_shmem_write(shm_bm_objid_t objid, u16_t data_offset, u16_t data_len);

int netmgr_udp_bind(u32_t ip_addr, u16_t port);

shm_bm_objid_t netmgr_udp_shmem_read(u16_t *data_offset, u16_t *data_len, u32_t *remote_addr, u16_t *remote_port);
int netmgr_udp_shmem_write(shm_bm_objid_t objid, u16_t data_offset, u16_t data_len, u32_t remote_ip, u16_t remote_port);

#endif /* NETMGR_H */
