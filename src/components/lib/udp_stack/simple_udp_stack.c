#include <cos_stubs.h>
#include <cos_types.h>
#include <string.h>
#include <arpa/inet.h>
#include <string.h>
#include <arpa/inet.h>
#include <netshmem.h>
#include <nic.h>
#include <net_stack_types.h>
#include "simple_udp_stack.h"

static u32_t host_ip;
static u16_t host_port;

static struct ether_addr nic_mac;
static struct ether_addr gw_mac = {
	.addr_bytes[0] = 0x52,
	.addr_bytes[1] = 0x9c,
	.addr_bytes[2] = 0xd1,
	.addr_bytes[3] = 0x13,
	.addr_bytes[4] = 0xa2,
	.addr_bytes[5] = 0x0e,
};

static inline void
udp_stack_packet_validate(struct ip_hdr *ip_hdr, u16_t packet_len, u32_t host_ip, u32_t host_port)
{
	struct udp_hdr *udp_hdr;

	assert(ip_hdr->version == IPv4);
	/* we don't support IP options, thus the ihl has to be 5 */
	assert(ip_hdr->ihl == 5);
	assert(ntohs(ip_hdr->total_len) == packet_len - ETH_STD_LEN);
	/* we don't support IP fragments */
	assert(ip_hdr->frag_off & 0x0040);
	assert(ip_hdr->ttl > 0);
	/* make sure it's a UDP proto */
	assert(ip_hdr->proto == UDP_PROTO);
	/* make sure it's going to host ip */
	assert(ip_hdr->dst_addr == host_ip);

	/* make sure it has correct csum */
	if (unlikely(!ENABLE_OFFLOAD)) {
		assert(udp_stack_ip_csum_calculate(ip_hdr) == 0);
	}

	udp_hdr = (struct udp_hdr *)((char *)ip_hdr + ip_hdr->ihl * 4);
	assert(udp_hdr->port.dst_port == host_port);

	if (unlikely(!ENABLE_OFFLOAD)) {
		assert(udp_stack_udp_cksum_verify(ip_hdr) == 0);
	}
}

static inline void
udp_stack_eth_hdr_set(struct eth_hdr *eth_hdr, struct ether_addr* src_mac, struct ether_addr* dst_mac)
{
	eth_hdr->src_addr = *src_mac;
	eth_hdr->dst_addr = *dst_mac;
	eth_hdr->ether_type = 0x0008;
}

static inline void
udp_stack_ip_hdr_set(struct ip_hdr *ip_hdr, u16_t data_len, u32_t src_host, u32_t dst_host)
{
	/* We don't support complex IP options */
	ip_hdr->ihl = IP_STD_LEN / 4;
	ip_hdr->version = IPv4;
	ip_hdr->tos = 0;
	ip_hdr->total_len = htons(IP_STD_LEN + data_len);
	ip_hdr->id = 0;
	ip_hdr->frag_off = 0;
	ip_hdr->ttl = 64;
	ip_hdr->proto = UDP_PROTO;
	ip_hdr->src_addr = src_host;
	ip_hdr->dst_addr = dst_host;
	ip_hdr->checksum = 0;
}

static inline void
udp_stack_udp_hdr_set(struct udp_hdr *udp_hdr, u16_t data_len, u16_t src_port, u16_t dst_port)
{
	udp_hdr->port.src_port = src_port;
	udp_hdr->port.dst_port = dst_port;
	udp_hdr->len = htons(data_len + 8);
	udp_hdr->checksum = 0;
}

static inline void
udp_stack_ip_csum_set(struct ip_hdr *ip_hdr)
{
	if (unlikely(!ENABLE_OFFLOAD))
		ip_hdr->checksum = udp_stack_ip_csum_calculate(ip_hdr);
}

static inline void
udp_stack_udp_csum_set(struct ip_hdr *ip_hdr)
{
	struct udp_hdr *udp_hdr = (struct udp_hdr *)((char *)ip_hdr + ip_hdr->ihl * 4);
	if (unlikely(!ENABLE_OFFLOAD))
		udp_hdr->checksum = udp_stack_udp_csum_calculate(ip_hdr);
}

static inline u16_t
udp_stack_hdr_room(void)
{
	return (ETH_STD_LEN + IP_STD_LEN + UDP_STD_LEN);
}

static inline struct ip_hdr *
udp_stack_ip_hdr_pos(struct eth_hdr *eth_hdr)
{
	return (struct ip_hdr *)((char *)eth_hdr + ETH_STD_LEN);
}

static inline struct udp_hdr *
udp_stack_udp_hdr_pos(struct ip_hdr *ip_hdr)
{
	return (struct udp_hdr *)((char *)ip_hdr + IP_STD_LEN);
}

void
udp_stack_shmem_map(cbuf_t shm_id)
{
	nic_shmem_map(shm_id);
	return;
}

int
udp_stack_udp_bind(u32_t ip_addr, u16_t port)
{
	/* Hold server's IP and port */
	u64_t mac;
	char tmp;
	host_ip = ip_addr;
	host_port = htons(port);
	mac = nic_get_port_mac_address(0);

	char *mac_addr = (char *)&mac;
	nic_mac.addr_bytes[0] = mac_addr[5];
	nic_mac.addr_bytes[1] = mac_addr[4];
	nic_mac.addr_bytes[2] = mac_addr[3];
	nic_mac.addr_bytes[3] = mac_addr[2];
	nic_mac.addr_bytes[4] = mac_addr[1];
	nic_mac.addr_bytes[5] = mac_addr[0];

	nic_bind_port(ip_addr, htons(port));

	return 0;
}

shm_bm_objid_t
udp_stack_shmem_read(u16_t *data_offset, u16_t *data_len, u32_t *remote_addr, u16_t *remote_port)
{
	shm_bm_objid_t           objid;
	struct netshmem_pkt_buf *obj;
	struct ip_hdr *ip_hdr;
	struct udp_hdr *udp_hdr;
	u16_t pkt_len;
	u16_t ip_len;

	objid = nic_get_a_packet(&pkt_len);
	obj = shm_bm_borrow_net_pkt_buf(netshmem_get_shm(), objid);
	assert(obj);

	ip_hdr = (struct ip_hdr *)(obj->data + ETH_STD_LEN);

	/* try to pass the validation */
	udp_stack_packet_validate(ip_hdr, pkt_len, host_ip, host_port);

	ip_len = ip_hdr->ihl * 4;
	udp_hdr = (struct udp_hdr *)((char *)ip_hdr + ip_len);

	*data_offset = ETH_STD_LEN + ip_len + UDP_STD_LEN;

	*data_len    = ntohs(udp_hdr->len) - UDP_STD_LEN;
	*remote_addr = ip_hdr->src_addr;
	*remote_port = udp_hdr->port.src_port;

	return objid;
}

int
udp_stack_shmem_write(shm_bm_objid_t objid, u16_t data_offset, u16_t data_len, u32_t remote_ip, u16_t remote_port)
{
	struct netshmem_pkt_buf *obj;
	u16_t pkt_offset, pkt_len;
	struct ip_hdr *ip_hdr;
	struct udp_hdr *udp_hdr;

	char *mac_addr_arr = (char *)&nic_mac;

	thdid_t thd = cos_thdid();
	char *data;

	obj  = shm_bm_borrow_net_pkt_buf(netshmem_get_shm(), objid);
	data = obj->data + data_offset;

	/* data now points to Eth hdr */
	data -= udp_stack_hdr_room();

	ip_hdr = udp_stack_ip_hdr_pos((struct eth_hdr *)data);
	udp_hdr = udp_stack_udp_hdr_pos(ip_hdr);

	udp_stack_udp_hdr_set(udp_hdr, data_len, host_port, remote_port);
	udp_stack_ip_hdr_set(ip_hdr, data_len + UDP_STD_LEN, host_ip ,remote_ip);
	udp_stack_ip_csum_set(ip_hdr);
	udp_stack_udp_csum_set(ip_hdr);
	udp_stack_eth_hdr_set((struct eth_hdr *)data, &nic_mac, &gw_mac);

	pkt_len = ntohs(ip_hdr->total_len) + ETH_STD_LEN;
	pkt_offset = netshmem_get_data_offset() - udp_stack_hdr_room();

	nic_send_packet(objid, pkt_offset, pkt_len);

	return 0;
}
