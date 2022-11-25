#ifndef SIMPLE_UDP_STACK_H
#define SIMPLE_UDP_STACK_H

#include <cos_types.h>
#include <shm_bm.h>
#include <net_stack_types.h>

/* checksum functions directly picked from DPDK */
static inline u32_t
__udp_stack_raw_cksum(const void *buf, size_t len, u32_t sum)
{
	/* extend strict-aliasing rules */
	typedef u16_t __attribute__((__may_alias__)) u16_p;
	const u16_p *u16_buf = (const u16_p *)buf;
	const u16_p *end = u16_buf + len / sizeof(*u16_buf);

	for (; u16_buf != end; ++u16_buf)
		sum += *u16_buf;

	/* if length is odd, keeping it byte order independent */
	if (unlikely(len % 2)) {
		u16_t left = 0;
		*(unsigned char *)&left = *(const unsigned char *)end;
		sum += left;
	}

	return sum;
}

static inline u16_t
__udp_stack_raw_cksum_reduce(u32_t sum)
{
	sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
	sum = ((sum & 0xffff0000) >> 16) + (sum & 0xffff);
	return (u16_t)sum;
}

static inline u16_t
udp_stack_raw_cksum(const void *buf, size_t len)
{
	u32_t sum;

	sum = __udp_stack_raw_cksum(buf, len, 0);
	return __udp_stack_raw_cksum_reduce(sum);
}

static inline u16_t
udp_stack_phdr_cksum(const struct ip_hdr *ip_hdr)
{
	struct ipv4_psd_header {
		u32_t src_addr; /* IP address of source host. */
		u32_t dst_addr; /* IP address of destination host. */
		u8_t  zero;     /* zero. */
		u8_t  proto;    /* L4 protocol type. */
		u16_t len;      /* L4 length. */
	} psd_hdr;

	u32_t l3_len;

	psd_hdr.src_addr = ip_hdr->src_addr;
	psd_hdr.dst_addr = ip_hdr->dst_addr;
	psd_hdr.zero = 0;
	psd_hdr.proto = ip_hdr->proto;
	
	l3_len = ntohs(ip_hdr->total_len);
	psd_hdr.len = ntohs((u16_t)(l3_len - ip_hdr->ihl * 4));

	return udp_stack_raw_cksum(&psd_hdr, sizeof(psd_hdr));
}

static inline u16_t
__udp_stack_udp_cksum(const struct ip_hdr *ip_hdr, const void *l4_hdr)
{
	u32_t cksum;
	u32_t l3_len, l4_len;
	u8_t ip_hdr_len;

	ip_hdr_len = ip_hdr->ihl * 4;
	l3_len = ntohs(ip_hdr->total_len);
	assert(l3_len > ip_hdr_len);

	l4_len = l3_len - ip_hdr_len;

	cksum = udp_stack_raw_cksum(l4_hdr, l4_len);
	cksum += udp_stack_phdr_cksum(ip_hdr);

	cksum = ((cksum & 0xffff0000) >> 16) + (cksum & 0xffff);

	return (u16_t)cksum;
}

static inline u16_t
udp_stack_udp_cksum(const struct ip_hdr *ip_hdr, const void *l4_hdr)
{
	u16_t cksum = __udp_stack_udp_cksum(ip_hdr, l4_hdr);

	cksum = ~cksum;

	/*
	 * Per RFC 768: If the computed checksum is zero for UDP,
	 * it is transmitted as all ones
	 * (the equivalent in one's complement arithmetic).
	 */
	if (cksum == 0 && ip_hdr->proto == UDP_PROTO)
		cksum = 0xffff;

	return cksum;
}

static inline int
udp_stack_udp_cksum_verify(const struct ip_hdr *ip_hdr)
{
	u16_t cksum = __udp_stack_udp_cksum(ip_hdr, (char *)ip_hdr + ip_hdr->ihl * 4);

	if (cksum != 0xffff)
		return -1;

	return 0;
}

static inline u16_t
udp_stack_ip_csum_calculate(struct ip_hdr *ip_hdr)
{
	u16_t cksum;
	cksum = udp_stack_raw_cksum(ip_hdr, ip_hdr->ihl * 4);
	return (u16_t)~cksum;
}

static inline u16_t
udp_stack_udp_csum_calculate(struct ip_hdr *ip_hdr)
{
	return udp_stack_udp_cksum(ip_hdr, (char *)ip_hdr + ip_hdr->ihl * 4);
}

void udp_stack_shmem_map(cbuf_t shm_id);
int udp_stack_udp_bind(u32_t ip_addr, u16_t port);
shm_bm_objid_t udp_stack_shmem_read(u16_t *data_offset, u16_t *data_len, u32_t *remote_addr, u16_t *remote_port);
int udp_stack_shmem_write(shm_bm_objid_t objid, u16_t data_offset, u16_t data_len, u32_t remote_ip, u16_t remote_port);

#endif /* SIMPLE_UDP_STACK_H */