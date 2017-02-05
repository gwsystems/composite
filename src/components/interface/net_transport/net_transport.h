#ifndef   	NET_TRANSPORT_H
#define   	NET_TRANSPORT_H

#include <cos_net.h>

net_connection_t net_create_tcp_connection(spdid_t spdid, u16_t tid, long evt_id);
net_connection_t net_create_udp_connection(spdid_t spdid, long evt_id);
net_connection_t net_accept(spdid_t spdid, net_connection_t nc);
int net_accept_data(spdid_t spdid, net_connection_t nc, long data);
int net_listen(spdid_t spdid, net_connection_t nc, int queue);
int net_bind(spdid_t spdid, net_connection_t nc, u32_t ip, u16_t port);
int net_connect(spdid_t spdid, net_connection_t nc, u32_t ip, u16_t port);
int net_close(spdid_t spdid, net_connection_t nc);
int net_send(spdid_t spdid, net_connection_t nc, void *data, int sz);
int net_recv(spdid_t spdid, net_connection_t nc, void *data, int sz);

#endif 	    /* !NET_TRANSPORT_H */
