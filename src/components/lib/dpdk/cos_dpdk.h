#ifndef COS_DPDK_H
#define COS_DPDK_H

#include <stdint.h> 
#include <stdbool.h>

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))

#define COS_ETH_NAME_SZ 16

/* vmxnet3 needs at least 1700 */
#define	COS_MBUF_DEFAULT_DATAROOM	1700
#define COS_PKTMBUF_HEADROOM 128

/* 
 * Be careful to set the mbuf size, we intentionally make the size less than 2k, 
 * so that one page(4k) can contain 2 mbufs, this enables DPDK to create more mbufs and 
 * save memory.
 * 
 * eg: currently we give 128M memory to DPDK. This can create around (128 * 1024 / 4k) * 2 = 65536
 * mbufs totally. The actually created mbufs in our tests is slightly less than this number, around 
 * 65000 mbufs. This is due to not all 128M memory is used as mempool memory. If you set the mbuf size
 * larger, you may end up with DPDK storing one mbuf per page(4k). This will drastically reduce the
 * number of mbufs DPDK can create.
 * 
 * In fact, if you don't want to support jumbo frame, this setting is enough since normal packet size
 * is less than 1500.
 */
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
/* DPDK physical/virtual NIC port identifier (which network interface card) */
typedef uint16_t cos_portid_t;        /* e.g., 0, 1, 2, 3 for different NICs */
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

/* 
 * Mempool opeartions are actually ring enqueue/dequeue opeartions,
 * the rts/hts mode enables multiple threads on a same core to operate
 * multiple consumers/producers, which means enable multiple threads on 
 * a same core to allocate/free mbufs safely.
 */
#define COS_MEMPOOL_DEFAULT_OPS (0)
#define COS_MEMPOOL_MP_MD_OPS "ring_mp_mc"
#define COS_MEMPOOL_MT_RTS_OPS "ring_mt_rts"
#define COS_MEMPOOL_MT_HTS_OPS "ring_mt_hts"

int cos_dpdk_init(int argc, char **argv);
uint16_t cos_eth_ports_init(void);

char* cos_create_pkt_mbuf_pool(const char *name, size_t nb_mbufs);
char* cos_create_pkt_mbuf_pool_by_ops(const char *name, size_t nb_mbufs, char* ops_name);
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

/* ext_shinfo: user needs to provide a small region within the external buffer to be used by DPDK storing data*/
int cos_attach_external_mbuf(char *mbuf, void *buf_vaddr,
			uint64_t buf_paddr, uint16_t buf_len,
			void (*ext_buf_free_cb)(void *addr, void *opaque),
			void *ext_shinfo);

void cos_set_external_packet(char*mbuf, uint16_t data_offset, uint16_t pkt_len, int offload);
int cos_mempool_full(const char *mp);
unsigned int cos_mempool_in_use_count(const char *mp);
int cos_eth_tx_done_cleanup(uint16_t port_id, uint16_t queue_id, uint32_t free_cnt);
uint64_t cos_get_port_mac_address(uint16_t port_id);

/* Port discovery and validation */
uint16_t cos_get_num_discovered_ports(void);
cos_portid_t cos_get_discovered_port(uint16_t index);
bool cos_port_is_valid(cos_portid_t port_id);

void cos_test_send(int queue, char *mp);

/* 
 * DPDK NIC Port Configuration
 * ===========================
 * These are the ACTUAL DPDK port IDs (not logical indices) for RX/TX operations.
 * 
 * How to find the correct port IDs:
 * 1. Run your application once and check the initialization logs from cos_eth_ports_init()
 * 2. Look for "DEV_INFO" sections showing port_id and dev_name for each NIC
 * 3. Use the port_id values shown in those logs
 * 4. If you cannot find the desired NIC, see COS_DPDK_DECLARE_NIC_MODULE
 * 
 * Common scenarios:
 * - Physical NICs: Check DPDK enumeration logs
 * - Virtual NICs (e.g., vmxnet3): Check DPDK logs for assigned port IDs
 * - To use same NIC for RX/TX: Set both to the same port ID
 * 
 * NOTE: The configured port IDs must exist in the system, otherwise
 *       initialization will fail with an assertion error.
 */
#define DPDK_NIC_PORT_RX  0   /* Actual DPDK port ID for receiving packets */
#define DPDK_NIC_PORT_TX  0   /* Actual DPDK port ID for transmitting packets */

/* NIC Queue Configuration per port */
#define NIC_RX_QUEUE_NUM 1
#define NIC_TX_QUEUE_NUM 1

#endif /* COS_DPDK_H */