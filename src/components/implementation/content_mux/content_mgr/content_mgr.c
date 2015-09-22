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

#include <content_mux.h>

/* FIXME: locking! */

typedef content_req_t (*content_open_fn_t)(spdid_t spdid, long evt_id, struct cos_array *data);
typedef int (*content_request_fn_t)(spdid_t spdid, content_req_t cr, struct cos_array *data);
typedef int (*content_retrieve_fn_t)(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more);
typedef int (*content_close_fn_t)(spdid_t spdid, content_req_t cr);

#include <static_content.h>
#include <async_inv.h>

/* reproduced as aliases from async_inv.h */
/* extern content_req_t alt_async_open(spdid_t spdid, long evt_id, struct cos_array *data); */
/* extern int alt_async_request(spdid_t spdid, content_req_t cr, struct cos_array *data); */
/* extern int alt_async_retrieve(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more); */
/* extern int alt_async_close(spdid_t spdid, content_req_t cr); */

extern content_req_t alt_static_open(spdid_t spdid, long evt_id, struct cos_array *data);
extern int alt_static_request(spdid_t spdid, content_req_t cr, struct cos_array *data);
extern int alt_static_retrieve(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more);
extern int alt_static_close(spdid_t spdid, content_req_t cr);


struct provider_fns {
	content_open_fn_t     open;
	content_request_fn_t  request;
	content_retrieve_fn_t retrieve;
	content_close_fn_t    close;
};

struct provider_fns static_content = {
	.open = static_open,
	.request = static_request,
	.retrieve = static_retrieve,
	.close = static_close
};

struct provider_fns alt_static_content = {
	.open = alt_static_open,
	.request = alt_static_request,
	.retrieve = alt_static_retrieve,
	.close = alt_static_close
};

/* struct provider_fns async_content = { */
/* 	.open = async_open, */
/* 	.request = async_request, */
/* 	.retrieve = async_retrieve, */
/* 	.close = async_close */
/* }; */

/* struct provider_fns alt_async_content = { */
/* 	.open = alt_async_open, */
/* 	.request = alt_async_request, */
/* 	.retrieve = alt_async_retrieve, */
/* 	.close = alt_async_close */
/* }; */

struct route {
	char *prefix;
	struct provider_fns *fns;
};

#define MAX_PATH 128

/* 
 * More specific (longer) prefixes should go first.  This lookup is
 * done like routing table lookups, from the top down, choosing
 * matches as they are found.  Thus even though a more general path
 * might exist that matches the path, it will not be chosen if a more
 * specific path exists above it.
 */
struct route routing_tbl[] = {
	/* { */
	/* 	.prefix = "/cgi/hw", */
	/* 	.fns = &async_content */
	/* }, */
	/* { */
	/* 	.prefix = "/cgi/HW", */
	/* 	.fns = &alt_async_content */
	/* }, */
	{
		.prefix = "/map",
		.fns = &alt_static_content
	},
	{
		.prefix = "/", 
		.fns = &static_content
	},
	{
		.prefix = "", 
		.fns = &static_content
	},
	{
		.prefix = NULL, 
		.fns = NULL
	}
};

static struct provider_fns *route_lookup(char *path, int sz)
{
	int i;

	assert(path);
	if (sz > MAX_PATH) return NULL;
	for (i = 0 ; routing_tbl[i].prefix != NULL ; i++) {
		struct route *r = &routing_tbl[i];
		int path_len, prefix_len;
		if (NULL == r->prefix) return NULL;

		path_len = strnlen(path, sz);
		prefix_len = strlen(r->prefix);
		if (path_len < prefix_len) continue;

		if (0 == strncmp(r->prefix, path, prefix_len)) {
			return r->fns;
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

content_req_t content_open(spdid_t spdid, long evt_id, struct cos_array *data)
{
	struct provider_fns *fns;
	struct content_req *r;

	if (!cos_argreg_arr_intern(data)) return -EINVAL;
	/* FIXME: should allow polling */
	if (evt_id < 0) return -EINVAL;

	fns = route_lookup(data->mem, data->sz);
	if (NULL == fns) return -EINVAL;
	r = request_alloc(fns, evt_id, spdid);
	if (NULL == r) return -ENOMEM;

	assert(fns && fns->open);
	r->child_id = fns->open(cos_spd_id(), evt_id, data);
	if (r->child_id < 0) {
		content_req_t err = r->child_id;
		printc("content_mgr: cannot open content w/ %s\n", data->mem);
		request_free(r);
		return err;
	}
	return r->id;
}

/* type = content_request_fn_t */
int content_request(spdid_t spdid, content_req_t cr, struct cos_array *data)
{
	struct content_req *r;

	r = request_find(cr);
	if (NULL == r) return -EINVAL;

	assert(r->fns && r->fns->request);
	return r->fns->request(cos_spd_id(), r->child_id, data);
}

/* type = content_retrieve_fn_t */
int content_retrieve(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more)
{
	struct content_req *r;

	r = request_find(cr);
	if (NULL == r) {
		printc("could not find request!\n");
		return -EINVAL;
	}

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
