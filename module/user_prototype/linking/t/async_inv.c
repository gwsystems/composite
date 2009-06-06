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

//#define ASYNC_TRACE
#ifdef ASYNC_TRACE
#define async_trace(x) printc(x)
#else
#define async_trace(x) 
#endif

extern int evt_trigger(spdid_t spdid, long extern_evt);

extern int sched_block(spdid_t spd_id);
extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id);

#define ASYNC_MAX_BUFFERED 512

typedef long content_req_t;

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

struct service_provider {
	char *name;
	long id;
	long evt_id;		/* evt to trigger when work becomes available */

	/* The message being currently processed by the server */
	struct message *curr_msg;
	struct message requests;
	int num_reqs;
	unsigned short int blocked_producer;
	
	struct service_provider *next, *prev;
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

static inline struct service_provider *provider_lookup(long id)
{
	return cos_map_lookup(&providers, id);
}

static void provider_prematurely_term_msg(struct message *m, struct service_provider *sp)
{
	assert(m && m->sp_id == sp->id);

	m->status = MSG_PROCESSED;
	m->ret_val = -EIO;
	if (evt_trigger(cos_spd_id(), m->evt_id)) assert(0);
}

static int provider_remove(long id)
{
	struct service_provider *sp;

	sp = provider_lookup(id);
	if (NULL == sp) return -EINVAL;
	assert(sp->id == id);
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
	if (sp->curr_msg) {
		provider_prematurely_term_msg(sp->curr_msg, sp);
		sp->curr_msg = NULL;
	}
	free(sp);
	
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
	
	id = cos_map_add(&providers, sp);
	if (0 > id) goto free_both;

	memset(sp, 0, sizeof(struct service_provider));
	INIT_LIST(sp, next, prev);
	ADD_LIST(&ps, sp, next, prev);
	INIT_LIST(&sp->requests, next, prev);
	sp->id = id;
	sp->evt_id = evt_id;
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
		struct service_provider *sp;

		assert(!EMPTY_LIST(m, next, prev));
		REM_LIST(m, next, prev);
		sp = provider_lookup(m->sp_id);
		assert(sp);
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
	UNLOCK();

	r = malloc(len);
	if (NULL == r) {
		printc("async_inv: could not allocate memory for request\n");
		err = -ENOMEM;
		goto err;
	}
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
	sp = provider_lookup(m->sp_id);
	if (NULL == sp) { 
		err = -EINVAL; 
		goto free; 
	}
	/* if there are too many requests currently queued up, block */
	if (sp->num_reqs >= ASYNC_MAX_BUFFERED) {
		/* FIXME: only a single producer for each service_provider */
		if (0 != sp->blocked_producer) assert(0);
		sp->blocked_producer = cos_get_thd_id();
		UNLOCK();
		sched_block(cos_spd_id());
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
	if (evt_trigger(cos_spd_id(), sp->evt_id)) assert(0);
	UNLOCK();

	return 0;
free:
	free(r);
err:
	UNLOCK();
	return err;
}

/* 
 * The server should use this to read a request off of the request
 * queue.  This will return a pointer to the request and its length
 * (in arguments).  If there are no requests, return EAGAIN.  If
 * error, return -EINVAL.  Set the current request in the service
 * provider to be the new request.
 *
 * FIXME: Each message must be read only once, and to its entirety.
 * This is not desirable behavior if e.g. the buffer passed in cannot
 * accommodate all of the message.
 */
static int provider_dequeue_request(content_req_t cr, char *req, int len)
{
	struct service_provider *sp;
	struct message *m;
	unsigned short int t;
	int ret;

	LOCK();
	sp = provider_lookup(cr);
	if (NULL == sp) goto err;
	if (EMPTY_LIST(&sp->requests, next, prev)) {
		UNLOCK();
		return 0;
	}
	/* this is the proxy ensuring that data for a message must be
	 * requested only once see fixme blow and above */
	if (sp->curr_msg) {
		UNLOCK();
		return 0;
	}

	sp->num_reqs--;
	m = FIRST_LIST(&sp->requests, next, prev);
	sp->curr_msg = m;
	m->status = MSG_PROCESSING;
	REM_LIST(m, next, prev);
	/* FIXME: use req_viewed to allow partial viewing */
	if (len < m->req_len) assert(0);
	memcpy(req, m->request, m->req_len);
	ret = m->req_len;
	m->req_viewed = m->req_len;

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
 * The reply is formulated and is to be enqueued in the
 * service_provider for later consumption.
 *
 * We copy rep here, so the caller should free it.
 */
static int provider_reply(content_req_t cr, char *rep, int len)
{
	struct service_provider *sp;
	struct message *m;
	char *r;
	int ret = 0;

	LOCK();
	sp = provider_lookup(cr);
	if (NULL == sp) {
		ret = -EINVAL;
		goto err;
	}

	m = sp->curr_msg;
	assert(m);
	sp->curr_msg = NULL;
	/* The message has been released, so delete it. */
	if (m->status == MSG_DELETE) {
		m->status = MSG_PROCESSED;
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
	if (evt_trigger(cos_spd_id(), m->evt_id)) assert(0);
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
	/* FIXME: Here we don't keep a direct reference in case the sp
	 * is later deallocated.  We avoid reference counting, but if
	 * the sp_id is later allocated to another provider, we make
	 * an incorrect call.  Should really do refcnting.
	 */
	m->sp_id = sp->id;
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
	UNLOCK();

	return sp->id;
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

