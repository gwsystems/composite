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

#include "map.h"

struct static_content {
	content_req_t id;
	long evt_id;
	int answer;
};

COS_MAP_CREATE_STATIC(static_requests);

char *parse_getreq(char *str)
{
	char *args = strchr(str, '?');
	
	if (!args) return NULL;
	if (!strncmp(args, "?id=", 4)) {
		return &args[4];
	}
	return NULL;
}

content_req_t static_open(spdid_t spdid, long evt_id, struct cos_array *data)
{
	struct static_content *sc = malloc(sizeof(struct static_content));
	content_req_t id;
	int i;
	
	//if (!cos_argreg_arr_intern(data)) return -EINVAL;

	if (NULL == sc) return -ENOMEM;
	id = cos_map_add(&static_requests, sc);
	if (id == -1) {
		free(sc);
		return -ENOMEM;
	}

	sc->answer = -1;
	if (data && cos_argreg_arr_intern(data)) {
		char *key;

		key = parse_getreq(data->mem);
		for (i = 0 ; key && map[i].key ; i++) {
			if (!strcmp(map[i].key, key)) sc->answer = i;
		}
	} else {
		printc("no data on open\n");
	}

	sc->id = id;
	sc->evt_id = evt_id;

	return id;
}

int static_request(spdid_t spdid, content_req_t cr, struct cos_array *data)
{
	struct static_content *sc;
	long evt_id;

	//if (!cos_argreg_arr_intern(data)) return -EINVAL;
	sc = cos_map_lookup(&static_requests, cr);
	if (NULL == sc) return -EINVAL;

	evt_id = sc->evt_id;
	/* Data available immediately */
	evt_trigger(cos_spd_id(), evt_id);

	return 0;
}

static const char msg[] = "Confirmation numbers not found\n";

int static_retrieve(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more)
{
	struct static_content *sc;

	if (!cos_argreg_arr_intern(data)) return -EINVAL;
	if (!cos_argreg_buff_intern((char*)more, sizeof(int))) return -EINVAL;

	sc = cos_map_lookup(&static_requests, cr);
	if (NULL == sc) return -EINVAL;

	if (sc->answer == -1) {
		strcpy(data->mem, msg);
		data->sz = strlen(msg);
		*more = 0;
	} else {
		strcpy(data->mem, map[sc->answer].value);
		data->sz = strlen(map[sc->answer].value);
		*more = 0;
	}

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
