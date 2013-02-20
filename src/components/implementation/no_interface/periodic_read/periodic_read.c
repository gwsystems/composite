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

/* svn/data/takoma-nov3-2009/PUBLIC/PUBLIC/ward1/MeetingThreeOutCodes.xml */
//const u32_t     IP      = 0x82555847; 		/* 130.85.88.71 */
const u32_t     IP      = 0x47585582; 		/* 130.85.88.71 */
const short int PORT    = 80;
const char     *REQUEST = 
"GET /svn/data/takoma-nov3-2009/PUBLIC/PUBLIC/ward1/MeetingThreeOutCodes.xml HTTP/1.0\r\n";

/* 
 * Response:
 *
 * HTTP/1.1 200 OK
 * ...
 * Content-Length: xxxx
 * ...
 * \t\r
 * data
 */

#define MEM_SZ (1024*1024-16)

//const int period = 1000;
const int period = 100;

int create_connection(net_connection_t *nc, long *eid)
{
	*eid = evt_create(cos_spd_id());
	assert(*eid >= 0);

	*nc = net_create_tcp_connection(cos_spd_id(), cos_get_thd_id(), *eid);
	printc("new connection %d\n", *nc);
	if (*nc < 0) BUG();
	if (net_connect(cos_spd_id(), *nc, IP, PORT)) BUG();

	return 0;
}

int delete_connection(net_connection_t nc, long eid)
{
	net_close(cos_spd_id(), nc);
	evt_free(cos_spd_id(), eid);

	return 0;
}

char *data_get(net_connection_t nc, long eid, int *sz)
{
	char *data, *msg;
	int ret, msg_sz = strlen(REQUEST), pos = 0;

	data = malloc(MEM_SZ);
	if (!data) return NULL;

	msg = cos_argreg_alloc(msg_sz);
	assert(msg);
	ret = net_send(cos_spd_id(), nc, msg, msg_sz);
	cos_argreg_free(msg);
	assert(ret == msg_sz);

	do {
again:
		assert(0); 	/* below 2 comments won't compile */
		evt_wait(cos_spd_id(), eid);
//		msg = cos_argreg_alloc(COS_MAX_ARG_SZ/2);
		assert(msg);
//		ret = net_recv(cos_spd_id(), nc, msg, COS_MAX_ARG_SZ/2);
		if (ret < 0) {
			cos_argreg_free(msg);
			printc("recv: %d\n", ret);
			goto again;
		}
		if (ret + pos > MEM_SZ) {
			cos_argreg_free(msg);
			free(data);
			return NULL;
		}
		memcpy(data+pos, msg, ret);
		cos_argreg_free(msg);
		pos += ret;
	} while (ret);
	
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

		printc("Creating connection...\n");
		create_connection(&nc, &eid);
		printc("...connection created...\n");
		data = data_get(nc, eid, &sz);
		printc("data pointer is %p\n", data);
		if (data) {
			free(data);
		}
		delete_connection(nc, eid);

		periodic_wake_wait(cos_spd_id());
	}
	return;
}
