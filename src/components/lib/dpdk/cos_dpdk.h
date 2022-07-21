#ifndef COS_DPDK_H
#define COS_DPDK_H

#include <stdint.h> 
#include <stdbool.h>

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))

#define COS_ETH_NAME_SZ 16

#define	COS_MBUF_DEFAULT_DATAROOM	2048
#define COS_PKTMBUF_HEADROOM 128
#define	COS_MBUF_DEFAULT_BUF_SIZE	\
	(COS_MBUF_DEFAULT_DATAROOM + COS_PKTMBUF_HEADROOM)

#define NIPQUAD(addr) \
((unsigned char *)&addr)[0], \
((unsigned char *)&addr)[1], \
((unsigned char *)&addr)[2], \
((unsigned char *)&addr)[3]

#define NIPQUAD_FMT "%u.%u.%u.%u"

#define NMACHEX(addr) \
((unsigned char *)&addr)[0], \
((unsigned char *)&addr)[1], \
((unsigned char *)&addr)[2], \
((unsigned char *)&addr)[3], \
((unsigned char *)&addr)[4], \
((unsigned char *)&addr)[5]

#define NMACHEX_FMT "%02x:%02x:%02x:%02x:%02x:%02x"

typedef uint8_t  cos_coreid_t;
typedef uint16_t cos_portid_t;
typedef uint16_t cos_queueid_t;

/* use opeaque pointer to hide details from DPDK to Composite app */

enum cos_dpdk_status_t {
	COS_DPDK_SUCCESS = 0,
	COS_DPDK_FALIURE = 1,
};

enum cos_dpdk_switch_t {
	COS_DPDK_SWITCH_OFF = 0,
	COS_DPDK_SWITCH_ON = 1,
};

int cos_dpdk_init(int argc, char **argv);
uint16_t cos_eth_ports_init(void);

char* cos_create_pkt_mbuf_pool(const char *name, size_t nb_mbufs);
int cos_free_packet(char* packet);

int cos_config_dev_port_queue(cos_portid_t port_id, uint16_t nb_rx_q, uint16_t nb_tx_q);
int cos_dev_port_adjust_rx_tx_desc(cos_portid_t port_id, uint16_t *nb_rx_desc, uint16_t *nb_tx_desc);

int cos_dev_port_rx_queue_setup(cos_portid_t port_id, uint16_t rx_queue_id, 
			uint16_t nb_rx_desc, char* mp);
int cos_dev_port_tx_queue_setup(cos_portid_t port_id, uint16_t tx_queue_id, 
			uint16_t nb_tx_desc);

int cos_dev_port_start(cos_portid_t port_id);
int cos_dev_port_stop(cos_portid_t port_id);
int cos_dev_port_set_promiscuous_mode(cos_portid_t port_id, bool mode);

uint16_t cos_dev_port_rx_burst(cos_portid_t port_id, uint16_t queue_id,
		 char**rx_pkts, const uint16_t nb_pkts);
uint16_t cos_dev_port_tx_burst(cos_portid_t port_id, uint16_t queue_id,
		 char**tx_pkts, const uint16_t nb_pkts);

void cos_get_port_stats(cos_portid_t port_id);

char* cos_get_packet(char* mbuf, int *len);
uint16_t cos_send_a_packet(char * pkt, uint32_t pkt_size, char* mp);
char* cos_allocate_mbuf(char* mp);

int cos_attach_external_mbuf(char *mbuf, void *buf_vaddr, uint16_t buf_len,
			void (*ext_buf_free_cb)(void *addr, void *opaque),
			uint64_t buf_paddr);

int cos_send_external_packet(char*mbuf, u16_t pkt_len);

#endif /* COS_DPDK_H */