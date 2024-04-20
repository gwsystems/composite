#ifndef NIC_NETIO_SHMEM_H
#define NIC_NETIO_SHMEM_H

#include <cos_types.h>
#include <cos_component.h>
#include <cos_stubs.h>
#include <shm_bm.h>

void nic_netio_shmem_map(cbuf_t shm_id);
int nic_netio_shmem_bind_port(u32_t ip_addr, u16_t port);
u64_t nic_netio_shmem_get_port_mac_address(u16_t port);
#endif
