/**
 * Copyright 2009 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial Author: Gabriel Parmer, gabep1@cs.bu.edu, 2009.
 */

#include <cos_component.h>
#include <cos_alloc.h>
#include <print.h>
#include <cos_map.h>
#include <errno.h>

//#include <content_provider.h>

/* FIXME: locking! */

typedef long content_req_t;

typedef content_req_t (*content_request_fn_t)(spdid_t spdid, long evt_id, struct cos_array *data);
typedef int (*content_retrieve_fn_t)(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more);
typedef int (*content_close_fn_t)(spdid_t spdid, content_req_t cr);

extern content_req_t static_request(spdid_t spdid, long evt_id, struct cos_array *data);
extern int static_retrieve(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more);
extern int static_close(spdid_t spdid, content_req_t cr);

struct provider_fns {
	content_request_fn_t  request;
	content_retrieve_fn_t retrieve;
	content_close_fn_t    close;
};

struct route {
	char *prefix;
	struct provider_fns fns;
};

#define MAX_PATH 128

struct route routing_tbl[] = {
	{
		.prefix = "/cgi", 
		.fns = {
			.request = NULL,
			.retrieve = NULL,
			.close = NULL
		}
	},
	{
		.prefix = "/", 
		.fns = {
			.request = static_request,
			.retrieve = static_retrieve,
			.close = static_close
		}
	},
	{
		.prefix = NULL, 
		.fns = {
			.request = NULL,
			.retrieve = NULL,
			.close = NULL
		}
	}
};

static inline int strnlen(char *s, int max)
{
	int i;

	for (i = 0 ; i < max ; i++) {
		if (s[i] == '\0') return i;
	}
	return max;
}

static struct provider_fns *route_lookup(char *path, int sz)
{
	int i;

	if (sz > MAX_PATH) return NULL;
	for (i = 0 ; routing_tbl[i].prefix != NULL ; i++) {
		struct route *r = &routing_tbl[i];
		int path_len, prefix_len;
		
		path_len = strnlen(path, sz);
		prefix_len = strlen(r->prefix);
		if (path_len != prefix_len) continue;
		if (0 == strncmp(r->prefix, path, path_len)) {
			return &r->fns;
		}
	}
	return NULL;
}

struct content_req {
	content_req_t id, child_id;

	long evt_id;
	spdid_t spdid;
	
	char *pending_data;
	int pending_sz;
	
	struct provider_fns *fns;
};

COS_MAP_CREATE_STATIC(content_requests);

static struct content_req *request_alloc(struct provider_fns *fns, long evt_id, spdid_t spdid)
{
	int id;
	struct content_req *r;

	assert(evt_id >= 0);
	assert(fns != NULL);

	r = malloc(sizeof(struct content_req));
	if (!r) return NULL;
	
	id = cos_map_add(&content_requests, r);
	if (-1 == id) {
		free(r);
		return NULL;
	}
	r->evt_id = evt_id;
	r->spdid = spdid;
	r->fns = fns;
	r->id = id;

	return r;
}

static inline struct content_req *request_find(content_req_t cr)
{
	assert(0 <= cr);
	return cos_map_lookup(&content_requests, cr);
}

static void request_free(struct content_req *req)
{
	cos_map_del(&content_requests, req->id);
	free(req);
}

/* type = content_request_fn_t */
content_req_t content_request(spdid_t spdid, long evt_id, struct cos_array *data)
{
	struct provider_fns *fns;
	struct content_req *r;

	if (!cos_argreg_arr_intern(data)) return -EINVAL;
	/* FIXME: should allow polling */
	if (evt_id < 0) return -EINVAL;

	fns = route_lookup(data->mem, data->sz);
	r = request_alloc(fns, evt_id, spdid);
	if (NULL == r) return -ENOMEM;

	assert(fns && fns->request);
	r->child_id = fns->request(cos_spd_id(), evt_id, data);
	if (r->child_id < 0) {
		content_req_t err = r->child_id;
		request_free(r);
		return err;
	}
	return r->id;
}

/* type = content_retrieve_fn_t */
int content_retrieve(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more)
{
	struct content_req *r;

	r = request_find(cr);
	if (NULL == r) return -EINVAL;

	assert(r->fns && r->fns->retrieve);
	return r->fns->retrieve(cos_spd_id(), r->child_id, data, more);
}

/* type = content_close_fn_t */
int content_close(spdid_t spdid, content_req_t cr)
{
	struct content_req *r;
	content_close_fn_t c;

	r = request_find(cr);
	if (NULL == r) return -EINVAL;
	assert(r->fns && r->fns->close);
	c = r->fns->close;
	request_free(r);
	
	return c(cos_spd_id(), r->child_id);
}

void cos_init(void *arg)
{
	cos_map_init_static(&content_requests);
	return;
}

void bin(void)
{
	extern int sched_block(spdid_t spdid);
	sched_block(cos_spd_id());
}
