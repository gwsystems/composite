#ifndef NIC_NETIO_SHMEM_H
#define NIC_NETIO_SHMEM_H

#include <cos_types.h>
#include <cos_component.h>
#include <cos_stubs.h>
#include <shm_bm.h>

/* Network TCP/UDP port number for application-level packet routing */
typedef u16_t net_service_port_t;      /* e.g., 80 (HTTP), 443 (HTTPS), custom ports */

void nic_netio_shmem_map(cbuf_t shm_id);
void nic_netio_shmem_map_bifurcate(u8_t dir, cbuf_t shm_id);
/* Bind thread to receive packets for specific IP address and network service port */
int nic_netio_shmem_bind_port(u32_t ip_addr, net_service_port_t service_port);
int nic_netio_shmem_bind_port_right(u32_t ip_addr, net_service_port_t service_port, u8_t right);
/* Get MAC address of DPDK NIC port (dpdk_port_id) */
u64_t nic_netio_shmem_get_port_mac_address(u16_t dpdk_port_id);
#endif
