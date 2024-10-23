#include <cos_types.h>
#include <cos_component.h>
#include <cos_stubs.h>
#include <netshmem.h>

void nic_shmem_map(cbuf_t shm_id){

}

int nic_send_packet(shm_bm_objid_t pktid, u16_t pkt_offset, u16_t pkt_len)
{
	return 0;
}
int nic_bind_port(u32_t ip_addr, u16_t port){
	return 0;
}

u64_t nic_get_port_mac_address(u16_t port){
	return 0;
}

shm_bm_objid_t nic_get_a_packet(u16_t *pkt_len){return 0;}