#ifndef COS_HEADERS_H
#define COS_HEADERS_H

#include <cos_types.h>

#define NIPQUAD(addr) \
((unsigned char *)&addr)[0], \
((unsigned char *)&addr)[1], \
((unsigned char *)&addr)[2], \
((unsigned char *)&addr)[3]
#define NIPQUAD_FMT "%u.%u.%u.%u"

struct eth_hdr {
	u8_t dst_addr[6];
	u8_t src_addr[6];
	u16_t ether_type;
} __attribute__((packed));

struct ether_addr {
	u8_t addr_bytes[6]; /**< Addr bytes in tx order */
} __attribute__((packed));

struct arp_ipv4 {
	struct ether_addr arp_sha;  /**< sender hardware address */
	u32_t             arp_sip;  /**< sender IP address */
	struct ether_addr arp_tha;  /**< target hardware address */
	u32_t             arp_tip;  /**< target IP address */
} __attribute__((packed));

struct arp_hdr {
	u16_t arp_hardware;    /* format of hardware address */
#define RTE_ARP_HRD_ETHER     1  /* ARP Ethernet address format */

	u16_t arp_protocol;    /* format of protocol address */
	u8_t  arp_hlen;    /* length of hardware address */
	u8_t  arp_plen;    /* length of protocol address */
	u16_t arp_opcode;     /* ARP opcode (command) */
#define	RTE_ARP_OP_REQUEST    1 /* request to resolve address */
#define	RTE_ARP_OP_REPLY      2 /* response to previous request */
#define	RTE_ARP_OP_REVREQUEST 3 /* request proto addr given hardware */
#define	RTE_ARP_OP_REVREPLY   4 /* response giving protocol address */
#define	RTE_ARP_OP_INVREQUEST 8 /* request to identify peer */
#define	RTE_ARP_OP_INVREPLY   9 /* response identifying peer */

	struct arp_ipv4 arp_data;
} __attribute__((packed));

struct ip_hdr {
	u8_t ihl:4;
	u8_t version:4;
	u8_t tos;
	u16_t total_len;
	u16_t id;
	u16_t frag_off;
	u8_t ttl;
	u8_t proto;
	u16_t checksum;
	u32_t src_addr;
	u32_t dst_addr;
} __attribute__((packed));

struct tcp_udp_port
{
	u16_t src_port;
	u16_t dst_port;
};

#endif