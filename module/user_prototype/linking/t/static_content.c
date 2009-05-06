/**
 * Copyright 2009 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2009.
 */

#include <cos_component.h>
#include <cos_alloc.h>
#include <print.h>
#include <cos_map.h>
#include <errno.h>

typedef long content_req_t;
extern int evt_trigger(spdid_t spdid, long extern_evt);

COS_MAP_CREATE_STATIC(static_requests);

content_req_t static_request(spdid_t spdid, long evt_id, struct cos_array *data)
{
	content_req_t id = cos_map_add(&static_requests, (void*)evt_id);

	if (id == -1) return -ENOMEM;
	evt_trigger(cos_spd_id(), evt_id);

	return id;
}

static const char msg[] = "All your base are belong to us";

int static_retrieve(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more)
{
	if (!cos_argreg_arr_intern(data)) return -EINVAL;
	if (!cos_argreg_buff_intern((char*)more, sizeof(int))) return -EINVAL;
	strcpy(data->mem, msg);
	data->sz = sizeof(msg);
	*more = 0;

	return 0;
}

int static_close(spdid_t spdid, content_req_t cr)
{
	cos_map_del(&static_requests, cr);
	return 0;
}

void cos_init(void *arg)
{
	cos_map_init_static(&static_requests);
	return;
}

void bin(void)
{
	extern int sched_block(spdid_t spdid);
	sched_block(cos_spd_id());
}
