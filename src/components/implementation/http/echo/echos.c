/**
 * Copyright 2011 by The George Washington University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author:  Gabriel Parmer, gparmer@gwu.edu, 2011
 */

#include <cos_component.h>
#include <cos_map.h>
#include <errno.h>
#include <evt.h>

#include <http.h>

struct connection {
	long conn_id;
	char *string;
	int strlen, off;
	long evt_id;
};

#define STR_SZ 1000

static struct connection *connection_alloc(long evt_id)
{
	struct connection *c = malloc(sizeof(struct connection));
	
	if (!c) return NULL;
	c->string = malloc(STR_SZ);
	if (!c->string) {
		free(c);
		return NULL;
	}
	c->strlen = STR_SZ;
	c->off = 0;
	c->string[0] = '\0';
	c->evt_id = evt_id;
	
	return c;
}

static void connection_free(struct connection *c)
{
	if (c->string) free(c->string);
	free(c);
}

COS_MAP_CREATE_STATIC(conn_map);

long content_split(spdid_t spdid, long conn_id, long evt_id)
{
	return -ENOSYS;
}

int content_write(spdid_t spdid, long connection_id, char *reqs, int sz)
{
	struct connection *c;		
	int ret = sz;

	if (sz < 1) return -EINVAL;
	c = cos_map_lookup(&conn_map, connection_id);
	if (NULL == c) return -EINVAL;
	
	if (c->off+sz < c->strlen) {
		memcpy(&c->string[c->off], reqs, sz);
		c->off += sz;
	} else {
		memcpy(&c->string[c->off], reqs, c->strlen - c->off);
		ret = c->strlen - c->off;
		c->off = c->strlen;
	}
	
	if (strchr(c->string, '\n') || c->off == c->strlen) {
		evt_trigger(cos_spd_id(), c->evt_id);
	}

	return sz;
}

int content_read(spdid_t spdid, long connection_id, char *buff, int sz)
{
	struct connection *c;
	int ret, i, j;
	
	c = cos_map_lookup(&conn_map, connection_id);
	if (NULL == c) return -EINVAL;

	ret = c->off > sz ? sz : c->off;
	memcpy(buff, c->string, ret);
	for (i = c->off, j = 0 ; j < ret && i < c->strlen ; i++, j++) {
		c->string[j] = c->string[i];
	}
	c->off -= ret;
	
	return ret;
}

long content_create(spdid_t spdid, long evt_id, struct cos_array *d)
{
	struct connection *c = connection_alloc(evt_id);
	long c_id;

	if (NULL == c) return -ENOMEM;
	c_id = cos_map_add(&conn_map, c);
	if (c_id < 0) {
		connection_free(c);
		return -ENOMEM;
	}
	c->conn_id = c_id;
	
	return c_id;
}

int content_remove(spdid_t spdid, long conn_id)
{
	struct connection *c = cos_map_lookup(&conn_map, conn_id);

	if (NULL == c) return 1;
	cos_map_del(&conn_map, c->conn_id);
	c->conn_id = -1;
	connection_free(c);

	return 0;
}

void cos_init(void *arg)
{
	cos_map_init_static(&conn_map);
	
	return;
}
