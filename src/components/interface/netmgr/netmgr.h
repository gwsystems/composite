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

void netmgr_shmem_init(cbuf_t rx_shm_id, cbuf_t tx_shm_id);

int netmgr_tcp_bind(u32_t ip_addr, u16_t port);

int netmgr_tcp_listen(u8_t backlog);
int netmgr_tcp_accept(struct conn_addr *client_addr);

// int net_tcp_read(void* buf, size_t sz);
// int net_tcp_write(void* buf, size_t sz);

shm_bm_objid_t netmgr_tcp_shmem_read();
int netmgr_tcp_shmem_write(shm_bm_objid_t objid);

// int net_udp_bind_port(u32_t ip_addr, u16_t port);

// int net_udp_read(void* buf, size_t sz);
// int net_udp_write(void* buf, size_t sz);

// int net_udp_read_with_shmem();
// int net_udp_write_with_shmem();

#endif /* NETMGR_H */
