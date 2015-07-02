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

#include <static_content.h>

#include <sched.h>
#include <evt.h>

struct static_content {
	content_req_t id;
	long evt_id;
};

COS_MAP_CREATE_STATIC(static_requests);

content_req_t static_open(spdid_t spdid, long evt_id, struct cos_array *data)
{
	struct static_content *sc = malloc(sizeof(struct static_content));
	content_req_t id;
	
	if (NULL == sc) return -ENOMEM;
	id = cos_map_add(&static_requests, sc);
	if (id == -1) {
		free(sc);
		return -ENOMEM;
	}
	sc->id = id;
	sc->evt_id = evt_id;

	return id;
}

int static_request(spdid_t spdid, content_req_t cr, struct cos_array *data)
{
	struct static_content *sc;
	long evt_id;

//	if (!cos_argreg_arr_intern(data)) return -EINVAL;

	sc = cos_map_lookup(&static_requests, cr);
	if (NULL == sc) return -EINVAL;
	evt_id = sc->evt_id;
	/* Data available immediately */
	evt_trigger(cos_spd_id(), evt_id);

	return 0;
}

static const char msg[] = 
	"<html><title>Scantegrity Verification</title>"
	"<body><form action=\"map\" method=\"get\">"
	"Voter Ballot ID: <input type=\"text\" name=\"id\">"
	"<input type=\"submit\" value=\"Get Confirmation\">"
	"</form></body></html>";

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
	struct static_content *sc;

	sc = cos_map_lookup(&static_requests, cr);
	if (NULL == sc) return -EINVAL;
	cos_map_del(&static_requests, cr);
	free(sc);

	return 0;
}

void cos_init(void *arg)
{
	cos_map_init_static(&static_requests);
	return;
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
