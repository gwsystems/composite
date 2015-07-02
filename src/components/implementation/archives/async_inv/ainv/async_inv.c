/**
 * Copyright 2009 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2009
 */

#include <cos_component.h>
#include <cos_alloc.h>
#include <print.h>
#include <cos_map.h>
#include <cos_list.h>
#include <errno.h>
#include <cos_synchronization.h>

#include <async_inv.h>

//#define ASYNC_TRACE
#ifdef ASYNC_TRACE
#define async_trace(x) printc(x)
#else
#define async_trace(x) 
#endif

#include <evt.h>
#include <sched.h>

#define ASYNC_MAX_BUFFERED 512

typedef enum {
	MSG_INIT,		/* created, but not yet make a request */
	MSG_PENDING,		/* request made and queued */
	MSG_PROCESSING,		/* currently being processed */
	MSG_PROCESSED,		/* done being processed, reply ready */
	MSG_DELETE		/* msg has been released, should delete asap */
} msg_status_t;

struct service_provider;

struct message {
	msg_status_t status;
	long id;
	/* The client's event to trigger when a reply is formulated */
	long evt_id;
	char *request, *reply;
	int req_len, rep_len;
	/* how much of the request or reply has been viewed, and */
	int req_viewed, rep_viewed;
	/* what should the return value to the client be? */
	int ret_val; 

	/* The id of the service provider */
	long sp_id;

	struct message *next, *prev; /* linked list */
};

typedef enum {
	PROVIDER_SPAWN_T = 1,
	PROVIDER_MAIN_T,
	PROVIDER_CLOSED_T
} provider_type_t;

struct service_provider;
struct provider_poly {
	provider_type_t type;
	long id;
	long evt_id;		/* evt to trigger when work becomes available */
	struct service_provider *sp;
	/* The message being currently processed by the server */
	struct message *curr_msg;
};

struct service_provider {
	struct provider_poly main;
	char *name;

	struct message requests;
	int num_reqs;
	unsigned short int blocked_producer;
	
	struct service_provider *next, *prev;

	struct provider_poly spawned;
};

/* a map of the service providers of type: id -> struct service_provider */
COS_MAP_CREATE_STATIC(providers);
/* a map of requests for these providers of type: id -> struct message */
COS_MAP_CREATE_STATIC(messages);
cos_lock_t biglock;
#define LOCK() lock_take(&biglock)
#define UNLOCK() lock_release(&biglock)

/* Linked list of the service providers...linearly walked to find the
 * correct .name associated with a request.  Type: char * -> struct
 * service_provider */
struct service_provider ps;

/* assumes next/prev are the pointers within the struct */
#define FOR_EACH(a, as) \
	for ((a) = FIRST_LIST((as), next, prev) ; \
	     (a) != (as) ;			  \
	     (a) = FIRST_LIST((a), next, prev) )

static struct service_provider *provider_find(char *name)
{
	struct service_provider *t;

	assert(name);
	FOR_EACH(t, &ps) {
		assert(t->name);
		if (0 == strcmp(t->name, name)) return t;
	}

	return NULL;
}

static inline struct provider_poly *provider_lookup(long id)
{
	return cos_map_lookup(&providers, id);
}

static void provider_prematurely_term_msg(struct message *m, struct service_provider *sp)
{
	assert(m && m->sp_id == sp->main.id);

	m->status = MSG_PROCESSED;
	m->ret_val = -EIO;
	if (evt_trigger(cos_spd_id(), m->evt_id)) BUG();
}

static int provider_main_remove(struct service_provider *sp)
{
	long id = sp->main.id;

	REM_LIST(sp, next, prev);
	cos_map_del(&providers, id);
	free(sp->name);

	/* Delete any pending messages. */
	while (!EMPTY_LIST(&sp->requests, next, prev)) {
		struct message *m = NULL;

		m = FIRST_LIST(&sp->requests, next, prev);
		provider_prematurely_term_msg(m, sp);
		REM_LIST(m, next, prev);
	}
	if (sp->spawned.type == PROVIDER_SPAWN_T && 
	    sp->spawned.curr_msg) {
		provider_prematurely_term_msg(sp->spawned.curr_msg, sp);
		sp->spawned.curr_msg = NULL;
	}

	/* 
	 * FIXME: this is crap -- remove the spawned id out from under
	 * it without a notification. 
	 */
	if (sp->spawned.type == PROVIDER_SPAWN_T) {
		cos_map_del(&providers, sp->spawned.id);
	}

	free(sp);

	return 0;
}

static int provider_remove(long id)
{
	struct provider_poly *poly;
	struct service_provider *sp;

	poly = provider_lookup(id);
	if (NULL == poly) return -EINVAL;
	switch(poly->type) {
	case PROVIDER_MAIN_T:
		sp = poly->sp;
		assert(sp->main.id == id);
		return provider_main_remove(sp);
	case PROVIDER_SPAWN_T:
		if (poly->id) cos_map_del(&providers, poly->id);
		poly->type = PROVIDER_CLOSED_T;
		break;
	case PROVIDER_CLOSED_T:
		break;
	}
	
	return 0;
}

static struct service_provider *provider_create(char *name, int len, long evt_id)
{
	struct service_provider *sp;
	long id;
	char *n;

	/* don't recreate providers */
	sp = provider_find(name);
	if (sp) goto err;
	sp = malloc(sizeof(struct service_provider));
	if (NULL == sp) goto err;
	n = malloc(len + 1);
	if (NULL == n) goto free_sp;
	memcpy(n, name, len);
	n[len] = '\0';

	id = cos_map_add(&providers, &sp->main);
	if (0 > id) goto free_both;

	memset(sp, 0, sizeof(struct service_provider));

	INIT_LIST(sp, next, prev);
	ADD_LIST(&ps, sp, next, prev);
	INIT_LIST(&sp->requests, next, prev);
	sp->main.type = PROVIDER_MAIN_T;
	sp->main.sp = sp;
	sp->main.id = id;
	sp->main.evt_id = evt_id;
	sp->spawned.type = PROVIDER_CLOSED_T;
	sp->spawned.sp = sp;
	sp->name = n;

	return sp;
free_both:
	free(n);
free_sp:
	free(sp);
err:
	return NULL;
}

/* 
 * Create a request message associated with a client.
 */
static struct message *provider_create_message(long evt_id)
{
	struct message *m;
	long mid;

	m = malloc(sizeof(struct message));
	if (NULL == m) return NULL;

	mid = cos_map_add(&messages, m);
	if (0 > mid) {
		free(m);
		return NULL;
	}

	/* initialize the message */
	memset(m, 0, sizeof(struct message));
	m->request = m->reply = NULL;
	m->req_len = m->rep_len = 0;

	m->evt_id = evt_id;
	m->id = mid;
	INIT_LIST(m, next, prev);
	m->status = MSG_INIT;

	return m;
}

/* 
 * Delete a given message.  The actual freeing of the object might be
 * done at a later point in time if it is currently being processed.
 */
static int provider_release_message(struct message *m)
{
	assert(m);

	switch (m->status) {
	case MSG_PENDING:
	{
		struct provider_poly *poly;
		struct service_provider *sp;

		assert(!EMPTY_LIST(m, next, prev));
		REM_LIST(m, next, prev);
		poly = provider_lookup(m->sp_id);
		assert(poly);
		assert(poly->type == PROVIDER_MAIN_T);
		sp = poly->sp;
		
		sp->num_reqs--;
		break;
	}
	case MSG_PROCESSING:
		/* can't delete it now, delete it when processing is
		 * done */
		m->status = MSG_DELETE;
	case MSG_DELETE:
		return 0;
	case MSG_PROCESSED:
	case MSG_INIT:
		break;
	}

	cos_map_del(&messages, m->id);
	if (m->reply) free(m->reply);
	if (m->request) free(m->request);
	free(m);

	return 0;
}

/* 
 * Take a request from the client, and enqueue it into the service
 * provider to be retrieved at a later point by the server.  Two edge
 * cases are if the buffer is full, we will block, and we will trigger
 * the service provider's event so that the server will know there is
 * work to be done.
 *
 * Might block.
 */
static int provider_enqueue_request(content_req_t cr, char *req, int len)
{
	struct message *m, *m_first;
	struct provider_poly *poly;
	struct service_provider *sp;
	int err = -ENOMEM;
	char *r;

	LOCK();
	/* get the message once before we sleep when it might change
	 * or be deallocated. */
	m_first = m = cos_map_lookup(&messages, cr);
	if (NULL == m || m->status != MSG_INIT) {
		err = -EINVAL;
		goto err;
	}

	r = malloc(len);
	if (NULL == r) {
		printc("async_inv: could not allocate memory for request\n");
		err = -ENOMEM;
		goto err;
	}
	UNLOCK();
retry:
	LOCK();
	m = cos_map_lookup(&messages, cr);
	/* 
	 * FIXME: This is both annoying and convoluted.  The message
	 * might have been deleted between the time we attempted to
	 * enqueue, which might have resulted in us sleeping, and now.
	 * Thus m might be a different m.  This is still WRONG as a
	 * pointer comparison only confirms that the new m is in the
	 * same location, not that it is the same m we were talking
	 * about before.  So this is broken, and to be fixed later.
	 */
	if (m != m_first) {
		err = -EINVAL;
		goto free;
	}
	poly = provider_lookup(m->sp_id);
	if (NULL == poly) { 
		err = -EINVAL; 
		goto free; 
	}
	assert(poly->type == PROVIDER_MAIN_T);
	sp = poly->sp;

	/* if there are too many requests currently queued up, block */
	if (sp->num_reqs >= ASYNC_MAX_BUFFERED) {
		/* FIXME: only a single producer for each service_provider */
		if (0 != sp->blocked_producer) BUG();
		sp->blocked_producer = cos_get_thd_id();
		UNLOCK();
		sched_block(cos_spd_id(), 0);
		goto retry;
	}
	
	memcpy(r, req, len);
	m->request = r;
	m->req_len = len;

	/* Manipulate the service provider to include the message */
	ADD_END_LIST(&sp->requests, m, next, prev);
	m->status = MSG_PENDING;
	sp->num_reqs++;
	/* Notify the provider that there is data pending */
	if (evt_trigger(cos_spd_id(), sp->main.evt_id)) BUG();
	UNLOCK();

	return 0;
free:
	free(r);
err:
	UNLOCK();
	return err;
}

/* 
 * The server should use this to read a request (using the cr of a
 * spawned handle) off of the request queue.  This will return a
 * pointer to the request and its length (in arguments).  If there are
 * no requests, return EAGAIN.  If error, return -EINVAL.  Set the
 * current request in the service provider to be the new request.
 */
static int provider_dequeue_request(content_req_t cr, char *req, int len)
{
	struct provider_poly *poly;
	struct service_provider *sp;
	struct message *m;
	unsigned short int t;
	int ret = 0, left, amnt;

	LOCK();
	poly = provider_lookup(cr);
	if (NULL == poly) goto err;
	if (poly->type != PROVIDER_SPAWN_T) goto err;
	sp = poly->sp;

	m = poly->curr_msg;
	assert(m);
	if (len < m->req_len) BUG();
	/* how much is there left to be read? */
	left = m->req_len - m->req_viewed;
	amnt = left > len ? len : left;
	if (0 < amnt) {
		memcpy(req, m->request, amnt);
		ret = amnt;
		m->req_viewed += amnt;
	}

	/* If there is a fully request queue, the client might blocked; wake it */
	t = sp->blocked_producer;
	if (0 != t) {
		sp->blocked_producer = 0;
		sched_wakeup(cos_spd_id(), t);
	}
	UNLOCK();

	return ret;
err:
	UNLOCK();
	return -EINVAL;
}

/* 
 * spawn a new request (analogous to accept in networking) that will
 * be used to read the message.  The cr must be a handle to a given
 * ascii string in the provider lookup namespace (not a spawn).
 */
static content_req_t provider_open_req_rep(content_req_t cr, long evt_id)
{
	struct provider_poly *poly, *spawn;
	struct service_provider *sp;
	struct message *m;
	
	poly = provider_lookup(cr);
	if (NULL == poly || poly->type != PROVIDER_MAIN_T) return -EINVAL;
	sp = poly->sp;

	if (EMPTY_LIST(&sp->requests, next, prev)) return -EAGAIN;

	spawn = &sp->spawned;
	spawn->type = PROVIDER_SPAWN_T;
	spawn->sp = sp;
	spawn->id = cos_map_add(&providers, spawn);
	if (0 > spawn->id) return -ENOMEM;
	spawn->evt_id = evt_id;

	sp->num_reqs--;
	m = FIRST_LIST(&sp->requests, next, prev);
	spawn->curr_msg = m;
	m->status = MSG_PROCESSING;
	REM_LIST(m, next, prev);
	m->req_viewed = 0;

	return spawn->id;
}

/* 
 * The reply is formulated and is to be enqueued in the
 * service_provider for later consumption.
 *
 * We copy rep here, so the caller should free it.
 */
static int provider_reply(content_req_t cr, char *rep, int len)
{
	struct provider_poly *poly;
	struct service_provider *sp;
	struct message *m;
	char *r;
	int ret = 0;

	LOCK();
	poly = provider_lookup(cr);
	if (NULL == poly || poly->type != PROVIDER_SPAWN_T) {
		ret = -EINVAL;
		goto err;
	}
	assert(cr == poly->id);
	sp = poly->sp;

	m = poly->curr_msg;
	assert(m);

	/* 
	 * If a reply has already been made, don't allow more.
	 * Really, we should allow the reply to be extended, but not
	 * yet.  Thus FIXME.
	 */
	if (m->reply) {
		printc("cannot reply twice to a request...\n");
		ret = -ENOMEM;
		goto err;
	}
	/* The message has been released, so delete it. */
	if (m->status == MSG_DELETE) {
		m->status = MSG_PROCESSED;
		poly->curr_msg = NULL;
		provider_release_message(m);
		goto err;
	}
	m->status = MSG_PROCESSED;
	m->ret_val = 0;
	m->reply = NULL;

	r = malloc(len + 1);
	if (r) {
		memcpy(r, rep, len);
		r[len] = '\0';
		m->reply = r;
		m->rep_len = len;
	} else {
		printc("async_inv: could not allocate reply.\n");
		m->ret_val = -ENOMEM;
	}
	if (evt_trigger(cos_spd_id(), m->evt_id)) BUG();
err:
	UNLOCK();
	return ret;
}

/* 
 * Read out the response for a given message.  If the message hasn't
 * been processed yet, return -EAGAIN.  The buffer to populate the
 * response into is rep, and its max length should be passed in len.
 * Return a negative error, or a positive length.
 */
static int provider_retrieve_reply(content_req_t cr, char *rep, int len, int *more)
{
	struct message *m;
	int ret = 0, min, left;

	LOCK();
	m = cos_map_lookup(&messages, cr);
	if (NULL == m) {
		ret = -EINVAL;
		goto fin;
	}

	/* not processed yet = data not currently available */
	if (m->status == MSG_PENDING) {
		ret = 0;//-EAGAIN;
		*more = 1;
		goto fin;
	}

	/* If we couldn't allocate the reply, convey that. */
	if (m->ret_val) {
		ret = m->ret_val;
		goto fin;
	}
	
	/* some of the reply might have already been read */
	left = m->rep_len - m->rep_viewed;
	min = len < left ? len : left;
	memcpy(rep, m->reply + m->rep_viewed, min);
	m->rep_viewed += min;
	if (m->rep_viewed == m->rep_len) *more = 0;
	else *more = 1;
	ret = min;
fin:
	UNLOCK();
	return ret;
}

/* 
 * The API for the client to make invocations/requests on that will be
 * asynchronously answered by the server.
 */

content_req_t async_open(spdid_t spdid, long evt_id, struct cos_array *data)
{
	struct message *m;
	struct service_provider *sp;

	async_trace("async_open\n");
	if (!cos_argreg_arr_intern(data)) return -EINVAL;
	if (data->mem[data->sz] != '\0') return -EINVAL;

	LOCK();
	m = provider_create_message(evt_id);
	if (NULL == m) {
		UNLOCK();
		printc("async_inv: open -- could not create message.");
		return -ENOMEM;
	}
	/* FIXME,BUG: stupid to trust that data->mem is bounded with \0  */
	sp = provider_find(data->mem);
	if (NULL == sp) {
		provider_release_message(m);
		UNLOCK();
		return -EINVAL;
	}
	assert(sp->main.type == PROVIDER_MAIN_T);
	/* FIXME: Here we don't keep a direct reference in case the sp
	 * is later deallocated.  We avoid reference counting, but if
	 * the sp_id is later allocated to another provider, we make
	 * an incorrect call.  Should really do refcnting.
	 */
	m->sp_id = sp->main.id;
	UNLOCK();

	return m->id;
}

int async_close(spdid_t spdid, content_req_t cr)
{
	struct message *m;

	async_trace("async_close\n");
	LOCK();
	m = cos_map_lookup(&messages, cr);
	if (NULL == m) {
		UNLOCK();
		return -EINVAL;
	}
	provider_release_message(m);
	UNLOCK();
	return 0;
}

int async_request(spdid_t spdid, content_req_t cr, struct cos_array *data)
{
	async_trace("async_request\n");
	if (!cos_argreg_arr_intern(data)) return -EINVAL;

	return provider_enqueue_request(cr, data->mem, data->sz);
}

int async_retrieve(spdid_t spdid, content_req_t cr, struct cos_array *data, int *more)
{
	int ret;

	async_trace("async_retrieve\n");
	if (!cos_argreg_arr_intern(data)) {
		printc("argument not in argreg!\n");
		return -EINVAL;
	}
	if (!cos_argreg_buff_intern((char*)more, sizeof(int))) {
		printc("more not in argreg\n");
		return -EINVAL;
	}

	ret = provider_retrieve_reply(cr, data->mem, data->sz, more);
	if (0 > ret) {
		data->sz = 0;
		return ret;
	}
	data->sz = ret;

	return 0;
}


/* 
 * The API for the content provider that is invoked with the
 * asynchronous requests.
 */

long content_create(spdid_t spdid, long evt_id, struct cos_array *data)
{
	struct service_provider *sp;

	async_trace("content_create\n");
	if (!cos_argreg_arr_intern(data)) return -EINVAL;
	if (data->mem[data->sz] != '\0') return -EINVAL;

	LOCK();
	sp = provider_create(data->mem, data->sz, evt_id);
	if (NULL == sp) {
		UNLOCK();
		return -EINVAL;
	}
	assert(sp->main.type == PROVIDER_MAIN_T);
	UNLOCK();

	return sp->main.id;
}

int content_remove(spdid_t spdid, long conn_id)
{
	int ret;

	async_trace("content_remove\n");
	LOCK();
	ret = provider_remove(conn_id);
	UNLOCK();
	
	return ret;
}

long content_split(spdid_t spdid, long conn_id, long evt_id)
{
	int ret;

	async_trace("content_split\n");

	LOCK();
	ret = provider_open_req_rep(conn_id, evt_id);
	UNLOCK();

	return ret;
}

/* 
 * FIXME: allow a reply to consist of more than the text of one call
 * to this function
 */
int content_write(spdid_t spdid, long conn_id, char *data, int sz)
{
	async_trace("content_write\n");
	if (!cos_argreg_buff_intern(data, sz)) return -EINVAL;
	
	return provider_reply(conn_id, data, sz);
}

/* 
 * FIXME: allow a reply to consist of more than the text of one call
 * to this function
 */
int content_read(spdid_t spdid, long conn_id, char *data, int sz)
{
	async_trace("content_read\n");
	if (!cos_argreg_buff_intern(data, sz)) return -EINVAL;

	return provider_dequeue_request(conn_id, data, sz);
}

void cos_init(void *arg)
{
	lock_static_init(&biglock);
	cos_map_init_static(&providers);
	cos_map_init_static(&messages);
	INIT_LIST(&ps, next, prev);
}
