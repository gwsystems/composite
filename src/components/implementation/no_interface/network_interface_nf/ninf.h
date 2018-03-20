#ifndef NINF_H
#define NINF_H

#include <stdio.h>
#include <string.h>

#include <cos_debug.h>
#include <llprint.h>
#include "pci.h"
#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

#undef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node)                                       \
	do {                                               \
		if (unlikely(!(node))) {                   \
			debug_print("assert error in @ "); \
            SPIN();                             \
		}                                          \
	} while (0)

#define SPIN()            \
	do {              \
		while (1) \
			; \
	} while (0)

/* DPDK data structures */
struct rte_mempool;
struct rte_mbuf;

/* DPDK functions */
extern int rte_eal_init(int, char**);
extern u8_t rte_eth_dev_count(void);
extern struct rte_mempool* rte_pktmbuf_pool_create(
        const char * name,
        unsigned n,
        unsigned cache_size,
        u16_t priv_size,
        u16_t data_room_size,
        int socket_id
);
extern int rte_eth_dev_cos_setup_ports(unsigned nb_ports,
        struct rte_mempool *mp);
/* extern u16_t rte_eth_rx_burst( */
/*         u16_t port_id, */
/*         u16_t queue_id, */
/*         struct rte_mbuf **rx_pkts, */
/*         const u16_t nb_pkts */
/* ); */
/* extern  u16_t rte_eth_tx_burst( */
/*         u16_t port_id, */
/*         u16_t queue_id, */
/*         struct rte_mbuf **tx_pkts, */
/*         const u16_t nb_pkts */
/* ); */

extern int rte_pktmbuf_alloc_bulk_cos(
        struct rte_mempool *pool,
        struct rte_mbuf **mbufs,
        unsigned count
);
extern u16_t rte_eth_rx_burst_cos(u8_t port_id, u16_t queue_id,
        struct rte_mbuf **rx_pkts, u16_t nb_pkts);
extern u16_t rte_eth_tx_burst_cos(u8_t port_id, u16_t queue_id,
        struct rte_mbuf **tx_pkts, u16_t nb_pkts);

#endif /* NINF_H */
