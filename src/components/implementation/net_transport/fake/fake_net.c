/**
 * Copyright 2008 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial author:  Gabriel Parmer, gabep1@cs.bu.edu, 2008
 */

#include <cos_component.h>
#include <errno.h>

#include <net_transport.h>

net_connection_t net_create_tcp_connection(spdid_t spdid, u16_t tid, long evt_id)
{
	return -ENOTSUP;
}

net_connection_t net_create_udp_connection(spdid_t spdid, long evt_id)
{
	return -ENOTSUP;
}

net_connection_t net_accept(spdid_t spdid, net_connection_t nc)
{
	return -ENOTSUP;
}

int net_accept_data(spdid_t spdid, net_connection_t nc, long data)
{
	return -ENOTSUP;
}

int net_listen(spdid_t spdid, net_connection_t nc, int queue)
{
	return -ENOTSUP;
}

int net_bind(spdid_t spdid, net_connection_t nc, u32_t ip, u16_t port)
{
	return -ENOTSUP;
}

int net_connect(spdid_t spdid, net_connection_t nc, u32_t ip, u16_t port)
{
	return -ENOTSUP;
}

int net_close(spdid_t spdid, net_connection_t nc)
{
	return -ENOTSUP;
}

int net_recv(spdid_t spdid, net_connection_t nc, void *data, int sz)
{
	return -ENOTSUP;
}

int net_send(spdid_t spdid, net_connection_t nc, void *data, int sz)
{
	return -ENOTSUP;
}

extern unsigned int sched_tick_freq(void);
unsigned int freq;
void bag(void)
{
	freq = sched_tick_freq();
}
