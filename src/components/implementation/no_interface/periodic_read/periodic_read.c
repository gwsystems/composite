/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <stdlib.h>
#include <string.h>

#include <cos_component.h>
#include <print.h>

#include <periodic_wake.h>
#include <net_transport.h>
#include <evt.h>
#include <sched.h>
#include <cos_alloc.h>

#define IP 0x00000000
#define PORT 80
#define REQUEST ""

#define MEM_SZ (1024*1024-16)

const int period = 1000;

int create_connection(net_connection_t *nc, long *eid)
{
	long eid;
	net_connection_t nc;

	*eid = evt_create(cos_spd_id());
	assert(eid >= 0);

	*nc = net_create_tcp_connection(cos_spd_id(), cos_get_thd_id(), *eid);
	if (*nc < 0) BUG();
	if (net_connect(cos_spd_id(), *nc, IP, PORT)) BUG();

	return 0;
}

int delete_connection(net_connection_t nc, long eid)
{
	net_close(cos_spd_id(), nc);
	evt_free(cos_spd_id(), eid);
}

char *data_get(net_connection_t nc, long eid, int *sz)
{
	char *data, *msg;
	int ret, msg_sz = strlen(REQUEST), pos = 0;

	data = malloc(MEM_SZ);
	if (!data) return NULL;

	msg = cos_argreg_alloc(msg_sz);
	assert(msg);
	ret = net_send(cos_spd_id(), nc, msg_sz);
	cos_argreg_free(msg);
	assert(ret == msg_sz);

	msg = cos_argreg_alloc(COS_MAX_ARG_SZ);
	assert(msg);
	do {
		evt_wait(eid);
		ret = net_recv(cos_spd_id(), nc, msg, COS_MAX_ARG_SZ);
		assert(ret >= 0);
		if (ret + pos > MEM_SZ) {
			free(data);
			return NULL;
		}
		memcpy(data+pos, msg, ret);
		pos += ret;
	} while (ret);
	cos_argreg_free(msg);
	
	*sz = pos;
	return data;
}

void cos_init(void *arg)
{
	if (period < 1) BUG();
	periodic_wake_create(cos_spd_id(), period);
	while (1) {
		long eid;
		net_connection_t nc;
		char *data;
		int sz;

		create_connection(&nc, &eid);
		data = data_get(nc, eid, &sz);
		if (data) {
			free(data);
		}
		delete_connection(nc, eid);

		periodic_wake_wait(cos_spd_id());
	}
	return;
}
