#ifndef NIC_H
#define NIC_H

#include <cos_types.h>
#include <cos_component.h>
#include <cos_stubs.h>

int nic_send_packet(char* pkt, size_t pkt_size);
int nic_bind_port(u32_t ip_addr, u16_t port, void* share_mem, size_t share_mem_sz);

#endif /* NIC_H */
