#ifndef NINF_UTIL_H
#define NINF_UTIL_H

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_mempool.h>
#include <rte_cycles.h>
#include <rte_ring.h>
#include <rte_ethdev.h>
#include <rte_ether.h>

void check_all_ports_link_status(uint8_t port_num, uint32_t port_mask);
/* 
 * Init ports!
 * */
int rte_eth_dev_cos_setup_ports(unsigned nb_ports, struct rte_mempool *mp);
void print_ether_addr(struct rte_mbuf *m);
#endif /* NINF_UTIL_H */
