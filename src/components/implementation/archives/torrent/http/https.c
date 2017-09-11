/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Change to the torrent interface, gparmer 2012
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

//#define COS_LINUX_ENV

#include <string.h>
#include <stdio.h>

#ifdef COS_LINUX_ENV

#include <stdlib.h>
#include <assert.h>
#include "cos_map.h"

#define printc printf

#define ENOMEM 2
#define EINVAL 3

#else  /* COS_LINUX_ENV */

#include <cos_component.h>
#include <cos_map.h>
#include <errno.h>

#include <torrent.h>
#include <torlib.h>
#include <cbuf.h>
#include <periodic_wake.h>
#include <sched.h>

static cos_lock_t h_lock;
#define LOCK() if (lock_take(&h_lock)) BUG();
#define UNLOCK() if (lock_release(&h_lock)) BUG();

extern td_t server_tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);
extern void server_trelease(spdid_t spdid, td_t tid);
extern int server_tread(spdid_t spdid, td_t td, int cbid, int sz);

#endif	/* COS_LINUX_ENV */

/* Keeping some stats (unsynchronized across threads currently) */
static volatile unsigned long http_conn_cnt = 0, http_req_cnt = 0;

inline static int is_whitespace(char s) { return ' ' == s; }

inline static char *remove_whitespace(char *s, int len)
{
	while (is_whitespace(*(s++)) && len--);
	return s-1;
}

inline static char *find_whitespace(char *s, int len)
{
	while (!is_whitespace(*(s++)) && len--);
	return s-1;
}

inline static int memcmp_fail_loc(char *s, char *c, int s_len,
				  int c_len, char **fail_loc)
{
	int min = (c_len < s_len) ? c_len : s_len;

	if (memcmp(s, c, min)) {
		*fail_loc = s;
		return 1;
	}
	*fail_loc = s + min;
	if (c_len > s_len) return 1;
	return 0;
}

static inline char *http_end_line(char *s, int len)
{
	if (len < 2) return NULL;
	if ('\r' == s[0] && '\n' == s[1]) return s+2;
	return NULL;
}

inline static int get_http_version(char *s, int len, char **ret, int *minor_version)
{
	char *c;
	char http_str[] = "HTTP/1.";
	int http_sz = sizeof(http_str)-1; // -1 for \n

	c = remove_whitespace(s, len);
	if (memcmp_fail_loc(c, http_str, len, http_sz, ret) ||
	    0 == (len - (*ret-s))) {
		*minor_version = -1;
		return 1;
	}
	c += http_sz;
	*minor_version = ((int)*c) - (int)'0';
	c++;
	if (http_end_line(c, len - (c-s))) c += 2;
	*ret = c;
	return 0;
}

static int http_find_next_line(char *s, int len, char **ret)
{
	char *r, *e;

	for (e = s;
	     len >= 2 && NULL == (r = http_end_line(e, len));
	     e++, len--);

	if (len >= 2) {
		*ret = r;
		return 0;
	} else {
		assert(len == 1 || len == 0);
		*ret = e+len;
		return 1;
	}
}

static int http_find_header_end(char *s, int len, char **curr, int flags)
{
	char *p, *c;

	c = s;
	do {
		p = c;

		/* parse the header and add in flags here */

		if (http_find_next_line(c, len-(int)(c-s), &c)) {
			/* Malformed: last characters not cr lf */
			*curr = c;
			return 1;
		}
	} while((int)(c - p) > 2);

	*curr = c;
	return 0;
}

typedef struct {
	char *end;
	char *path;
	int path_len;
	int head_flags;
} get_ret_t;

static int http_get_parse(char *req, int len, get_ret_t *ret)
{
	char *s = req, *e, *end;
	char *path;
	int path_len, minor_version;

	end = req + len;

	if (memcmp_fail_loc(s, "GET ", len, 4, &e)) goto malformed_request;

	path = remove_whitespace(e, len);
	e = find_whitespace(path, len - (path-s));
	path_len = e-path;
	e = remove_whitespace(e, len - (e-s));

	if (get_http_version(e, len - (e-s), &e, &minor_version)) goto malformed_request;
	if (minor_version != 0 && minor_version != 1) {
		/* This should be seen as an error: */
		e = s;
		goto malformed_request;
	}

	ret->head_flags = 0;
	if (http_find_header_end(e, len - (e-s), &e, ret->head_flags)) goto malformed_request;

	ret->end = e;
	ret->path = path;
	ret->path_len = path_len;
	return 0;

malformed_request:
	ret->path = NULL;
	ret->path_len = -1;
	ret->end = e;
	return -1;
}

struct http_response {
	char *resp;
	int resp_len;
};

struct connection {
	int refcnt;
	td_t conn_id;
	long evt_id;
	cos_lock_t lock;
	struct http_request *pending_reqs;
};

static inline void
lock_connection(struct connection *c) { if (lock_take(&c->lock)) BUG(); }
static inline void
unlock_connection(struct connection *c) { if (lock_release(&c->lock)) BUG(); }

/*
 * The queue of requests should look like so:
 *
 * c
 * |
 * V
 * p<->p<->p<->.<->.<->.<->P
 *
 * c: connection, P: request with pending data, .: requests that have
 * not been processed (requests made to content containers), p:
 * requests that have been processed (sent to content managers), but
 * where the reply has not been transferred yet.  In most cases, P
 * will also have the malloc flag set.
 */
#define HTTP_REQ_PENDING      0x1 /* incomplete request, pending more
				   * data */
#define HTTP_REQ_MALLOC       0x2 /* the buffer has been malloced */
#define HTTP_REQ_PROCESSED    0x4 /* request not yet made */

enum {HTTP_TYPE_TOP, 
      HTTP_TYPE_GET};

struct http_request {
	long id, content_id;
	int flags, type;
	struct connection *c;
	struct http_response resp;

	char *req, *path;
	int req_len, path_len;

	struct http_request *next, *prev;
};

static int http_get_request(struct http_request *r)
{
	int ret = -1;
	assert(r && r->c);

	if (0 > r->content_id) {
		r->content_id = server_tsplit(cos_spd_id(), td_root, r->path, 
					      r->path_len, TOR_READ, r->c->evt_id);
		if (r->content_id < 0) return r->content_id;
		ret = 0;
	}
	return ret;
}

static int http_get(struct http_request *r)
{
	get_ret_t ret;
	char *s = r->req, *p;
	int len = r->req_len;

	if (http_get_parse(s, len, &ret)) {
		r->req_len = (int)(ret.end - s);
		return -1;
	}
	p = malloc(ret.path_len+1);
	if (!p) {
		printc("path could not be allocated\n");
		r->req_len = 0;
		return -1;
	}
	memcpy(p, ret.path, ret.path_len);
	p[ret.path_len] = '\0';

	r->path = p;
	r->path_len = ret.path_len;
	r->req_len = (int)(ret.end - s);
	r->flags |= ret.head_flags;
	return 0;
}

static int http_parse_request(struct http_request *r)
{
	if (!memcmp("GET", r->req, 3)) {
		r->type = HTTP_TYPE_GET;
		return http_get(r);
	}
	printc("unknown request type for message\n<<%s>>\n", r->req);
	return -1;
}

static int http_make_request(struct http_request *r)
{
	switch (r->type) {
	case HTTP_TYPE_GET:
		return http_get_request(r);
		break;
	default:
		printc("unknown request type\n");
		return -1;
	}

	return 0;
}

static struct connection *http_new_connection(long conn_id, long evt_id)
{
	struct connection *c = malloc(sizeof(struct connection));

	if (NULL == c) return c;
	c->conn_id = conn_id;
	c->evt_id = evt_id;
	c->pending_reqs = NULL;
	c->refcnt = 1;
	lock_static_init(&c->lock);

	return c;
}

static void http_free_request(struct http_request *r);

static inline void conn_refcnt_dec(struct connection *c)
{
	c->refcnt--;
	if (0 == c->refcnt) {
		struct http_request *r, *first, *next;

		if (NULL != c->pending_reqs) {
			first = r = c->pending_reqs;
			do {
				next = r->next;
				/* FIXME: don't do this if the request
				 * has been made external to this
				 * component. */
				if (!(r->flags & HTTP_REQ_PROCESSED)) {
					http_free_request(r);
				}
				
				r = next;
			} while (first != r);
		}
		lock_static_free(&c->lock);
		free(c);
	}
}

static void http_free_connection(struct connection *c)
{
	conn_refcnt_dec(c);
}

static inline void http_init_request(struct http_request *r, int buff_len, struct connection *c, char *req)
{
	static long id = 0;

	memset(r, 0, sizeof(struct http_request));
	r->content_id = -1;
	r->id = id++;
	r->type = HTTP_TYPE_TOP;
	r->c = c;
	c->refcnt++;
	r->req = req;
	r->req_len = buff_len;
	if (c->pending_reqs) {
		struct http_request *head = c->pending_reqs, *tail = head->prev;

		/* Add the new request to the end of the queue */
		r->next = head;
		r->prev = tail;
		tail->next = r;
		head->prev = r;
	} else {
		r->next = r->prev = r;
		c->pending_reqs = r;
	}
}

static struct http_request *http_new_request_flags(struct connection *c, char *req, int buff_len, int flags)
{
	struct http_request *r = malloc(sizeof(struct http_request));

	if (NULL == r) return r;
	http_init_request(r, buff_len, c, req);
	r->flags = flags;

	return r;
}

static struct http_request *http_new_request(struct connection *c, char *req, int buff_len)
{
	return http_new_request_flags(c, req, buff_len, 0);
}

static void __http_free_request(struct http_request *r)
{
	/* FIXME: don't free response if in arg. reg. */
	if (r->resp.resp) free(r->resp.resp);
	if (r->path) free(r->path);
	assert(r->req);
	if (r->flags & HTTP_REQ_MALLOC) free(r->req);
	free(r);
}

static void http_free_request(struct http_request *r)
{
	struct connection *c = r->c;
	struct http_request *next = r->next, *prev = r->prev;

	assert(c->pending_reqs);
	if (r->next != r) {
		next->prev = r->prev;
		prev->next = r->next;
	} //else assert(r->prev == r && c->pending_reqs == r);
	r->next = r->prev = NULL;
	//assert(c->pending_reqs == r);
	if (c->pending_reqs == r) {
		c->pending_reqs = (r == next) ? NULL : next;
	}
	server_trelease(cos_spd_id(), r->content_id);
	conn_refcnt_dec(c);
	__http_free_request(r);
}

/* 
 * This is annoying, but the caller needs to know the buffer size of
 * the request that we are using.  In most cases, the passed in buffer
 * is what we use, so it isn't an issue, but if we malloc our own
 * buffer (see the HTTP_REQ_PENDING case), then the total buffer size
 * can grow.  Thus the buff_sz argument.
 */
static struct http_request *connection_handle_request(struct connection *c, char *buff,
						      int amnt, int *buff_sz)
{
	struct http_request *r, *last;

	*buff_sz = amnt;
	r = http_new_request(c, buff, amnt);
	if (NULL == r) return NULL;
	last = r->prev;

	/* 
	 * If a previous request required more data to parse correctly
	 * (was pending), then we need to combine its buffer with the
	 * current one and try to parse again.
	 */
	if (last != r && last->flags & HTTP_REQ_PENDING) {
		char *new_buff;
		int new_len;

		if (last->prev != last && last->prev->flags & HTTP_REQ_PENDING) {
			http_free_request(r);
			return NULL;
		}
		new_len = amnt + last->req_len;
		new_buff = malloc(new_len + 1);
		if (NULL == new_buff) {
			printc("malloc fail 1\n");
			http_free_request(r);
			return NULL;
		}
		memcpy(new_buff, last->req, last->req_len);
		memcpy(new_buff + last->req_len, buff, amnt);
		buff = new_buff;
		*buff_sz = amnt = new_len;
		new_buff[new_len] = '\0';
		http_free_request(last);

		if (r->flags & HTTP_REQ_MALLOC) free(r->req);
		r->req_len = new_len;
		r->req = new_buff;
		r->flags |= HTTP_REQ_MALLOC;
	}

	/*
	 * Process the list of requests first, then go through and
	 * actually make the requests.
	 */
	if (http_parse_request(r)) {
		char *save_buff;

		/* parse error?! Parsing broke somewhere _in_ the
		 * message, so we need to report this as an error */
		if (r->req_len != amnt) {
			/* FIXME: kill connection */
			http_free_request(r);
			return NULL;
		}		
		assert(r->req_len == amnt && r->req == buff);
		/*
		 * If we failed because we simply don't have a whole
		 * message (more data is pending), then store it away
		 * appropriately.
		 */
		save_buff = malloc(amnt + 1);
		if (NULL == save_buff) {
			printc("malloc fail 2\n");
			/* FIXME: kill connection */
			http_free_request(r);
			return NULL;
		}
		memcpy(save_buff, buff, amnt);
		save_buff[amnt] = '\0';
		if (r->flags & HTTP_REQ_MALLOC) free(r->req);
		r->req = save_buff;
		r->flags |= (HTTP_REQ_PENDING | HTTP_REQ_MALLOC);
	}
	return r;
}

static int connection_parse_requests(struct connection *c, char *req, int req_sz)
{
	struct http_request *r, *first;

	/* Parse all the requests in the buffer */
	while (req_sz > 0) {
		int start_amnt;

		r = connection_handle_request(c, req, req_sz, &start_amnt);
		/* TODO: here we should check if an error occurred and
		 * the response can be sent.  If so set
		 * HTTP_REQ_PROCESSED. */
		if (NULL == r) return 1;
		req = r->req + r->req_len;
		req_sz = start_amnt - r->req_len;
	}
	
	/* Now submit those requests (process them) */
	first = r = c->pending_reqs;
	if (NULL == r) return 0;
	do {
		if (!(r->flags & (HTTP_REQ_PENDING | HTTP_REQ_PROCESSED))) {
			if (http_make_request(r)) {
				printc("https: Could not process response.\n");
				return -1;
			}
			if (!(r->flags & HTTP_REQ_PENDING)) r->flags |= HTTP_REQ_PROCESSED;
		}
		r = r->next;
	} while (r != first);
	
	return 0;
}

static const char success_head[] =
	"HTTP/1.1 200 OK\r\n"
	"Date: Mon, 01 Jan 1984 00:00:01 GMT\r\n"
	"Content-Type: text/html\r\n"
	"Connection: close\r\n"
	"Content-Length: ";
//static const char resp[] = "all your base are belong to us\r\n";

#define MAX_SUPPORTED_DIGITS 20

/* Must prefix data by "content_length\r\n\r\n" */
static int http_get_header(char *dest, int max_len, int content_len, int *resp_len)
{
	int resp_sz = content_len;
	int head_sz = sizeof(success_head)-1;
	int tot_sz, len_sz = 0;
	char len_str[MAX_SUPPORTED_DIGITS];

	len_sz = snprintf(len_str, MAX_SUPPORTED_DIGITS, "%d\r\n\r\n", resp_sz);
	if (MAX_SUPPORTED_DIGITS == len_sz || len_sz < 1) {
		printc("length of response body too large\n");
		*resp_len = 0;
		return -1;
	}

	tot_sz = head_sz + len_sz + resp_sz;
	/* +1 for \0 so we can print the string */
	if (tot_sz + 1 > max_len) {
		*resp_len = 0;
		return 1;
	}
	memcpy(dest, success_head, head_sz);
	dest += head_sz;
	memcpy(dest, len_str, len_sz);
	dest += len_sz;
	*resp_len = len_sz + head_sz;
	dest[0] = '\0';

	return 0;
}

static int connection_get_reply(struct connection *c, char *resp, int resp_sz)
{
	struct http_request *r;
	int used = 0;

	/* 
	 * Currently, this doesn't do anything interesting.  In the
	 * future it will call the content provider and get the
	 * (ready) response.
	 */
	r = c->pending_reqs;
	if (NULL == r) return 0;
	while (r) {
		struct http_request *next;
		char *local_resp;
		cbuf_t cb;
		int consumed, ret, local_resp_sz;

		assert(r->c == c);
		if (r->flags & HTTP_REQ_PENDING) break;
		assert(r->flags & HTTP_REQ_PROCESSED);
		assert(r->content_id >= 0);

		/* Previously saved response? */
		if (NULL != r->resp.resp) {
			local_resp = r->resp.resp;
			local_resp_sz = r->resp.resp_len;
		} else {
			int sz;
			/* Make the request to the content
			 * component */
			sz         = resp_sz - used;
			local_resp = cbuf_alloc_ext(sz, &cb, CBUF_TMEM);
			if (!local_resp) BUG();

			ret = server_tread(cos_spd_id(), r->content_id, cb, sz);
			if (ret < 0) {
				cbuf_free(cb);
				printc("https get reply returning %d.\n", ret);
				return ret;
			}
			local_resp_sz = ret;
		}

		/* no more data */
		if (local_resp_sz == 0) {
			cbuf_free(cb);
			break;
		}

		/* If the header and data couldn't fit into the
		 * provided buffer, then we need to save the response,
		 * so that we can send it out later... */
		if (http_get_header(resp+used, resp_sz-used, local_resp_sz, &consumed)) {
			if (NULL == r->resp.resp) {
				char *save;
			
				save = malloc(local_resp_sz);
				assert(save);
				assert(local_resp);
				memcpy(save, local_resp, local_resp_sz);
				cbuf_free(cb);
				local_resp = NULL;

				r->resp.resp = save;
				r->resp.resp_len = local_resp_sz;
			}
			if (0 == used) {
				printc("https: could not allocate either header or response of sz %d:%s\n", local_resp_sz, local_resp);
				if (local_resp) cbuf_free(cb);
				return -ENOMEM;
			}
			break;
		}

		memcpy(resp+used+consumed, local_resp, local_resp_sz);
		
		assert(local_resp);
		cbuf_free(cb);
		local_resp = NULL;

		used += local_resp_sz + consumed;
		next = r->next;
		/* bookkeeping */
		http_req_cnt++;

		http_free_request(r);
		r = c->pending_reqs;
		assert(r == next || NULL == r);
	}

	return used;
}


// ~/research/others_software/httperf-0.9.0/src/httperf --port=200 --wsess=10000,20,0 
// --burst-len=20 --rate=2000 --server=10.0.2.8 --max-piped-calls=32 --uri=/cgi/hw

// ./ab -c 32 -n 66000 10.0.2.8:200/cgi/hw ;
// ./ab -c 32 -n 66000 10.0.2.8:200/ ; 
// ./ab -c 32 -n 66000 10.0.2.8:200/cgi/hw2

/* static int connection_process_requests(struct connection *c, char *req, int req_sz, */
/* 				char *resp, int resp_sz) */
/* { */
/* 	/\* FIXME: close connection on error? *\/ */
/* 	if (connection_parse_requests(c, req, req_sz)) return -EINVAL; */
/* 	return connection_get_reply(c, resp, resp_sz); */
/* } */

/* static int http_read_write(spdid_t spdid, long connection_id, char *reqs, int req_sz, char *resp, int resp_sz) */
/* { */
/* 	struct connection *c; */
	
/* 	c = cos_map_lookup(&conn_map, connection_id); */
/* 	if (NULL == c) return -EINVAL; */

/* 	return connection_process_requests(c, reqs, req_sz, resp, resp_sz); */
/* } */

td_t 
tsplit(spdid_t spdid, td_t tid, char *param, int len, 
       tor_flags_t tflags, long evtid)
{
	td_t ret = -1;
	struct torrent *t;
	struct connection *c;

	if (tor_isnull(tid)) return -EINVAL;
	
	LOCK();
	c = http_new_connection(0, evtid);
	if (!c) ERR_THROW(-ENOMEM, err);
	/* ignore the param for now */
	t = tor_alloc(c, tflags);
	if (!t) ERR_THROW(-ENOMEM, free);
	c->conn_id = ret = t->td;
err:
	UNLOCK();
	return ret;
free:
	http_free_connection(c);
	goto err;
}

void
trelease(spdid_t spdid, td_t td)
{
	struct torrent *t;
	struct connection *c;

	if (!tor_is_usrdef(td)) return;

	LOCK();
	t = tor_lookup(td);
	if (!t) goto done;
	c = t->data;
	lock_connection(c);
	/* wait till others release the connection */
	unlock_connection(c);
	if (c) {
		http_free_connection(c);
		/* bookkeeping */
		http_conn_cnt++;
	}
	tor_free(t);
done:
	UNLOCK();
	return;
}

int 
tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	int ret = 0;

	/* currently only allow deletion */
	if (td_into != td_null) ERR_THROW(-EINVAL, done);
done:
	return ret;
}

int 
twrite(spdid_t spdid, td_t td, int cbid, int sz)
{
	struct connection *c = NULL;
	struct torrent *t;
	char *buf;
	int ret = -1;

	if (tor_isnull(td)) return -EINVAL;
	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, unlock);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, unlock);

	c = t->data;
	assert(c);

	lock_connection(c);
	UNLOCK();
	if (connection_parse_requests(c, buf, sz)) ERR_THROW(-EINVAL, release);
	unlock_connection(c);
	ret = sz;
done:
	return ret;
unlock:
	UNLOCK();
release:
	unlock_connection(c);
	goto done;
}

int 
tread(spdid_t spdid, td_t td, int cbid, int sz)
{
	struct connection *c;
	struct torrent *t;
	char *buf;
	int ret;
	
	if (tor_isnull(td)) return -EINVAL;
	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, unlock);
	assert(!tor_is_usrdef(td) || t->data);
	if (!(t->flags & TOR_READ)) ERR_THROW(-EACCES, unlock);
	c = t->data;

	lock_connection(c);
	UNLOCK();
	ret = connection_get_reply(c, buf, sz);
	unlock_connection(c);
done:	
	return ret;
unlock:
	UNLOCK();
	goto done;
}

/* long  */
/* content_split(spdid_t spdid, long conn_id, long evt_id) */
/* { */
/* 	return -ENOSYS; */
/* } */

/* int  */
/* content_write(spdid_t spdid, long connection_id, char *reqs, int sz) */
/* { */
/* 	struct connection *c; */
/* 	struct torrent *t; */
/* 	cbuf_t cb; */
/* 	char *cbuf; */
/* //     printc("HTTP write"); */
	
/* 	t = tor_lookup(connection_id); */
/* 	assert(t); */
/* 	c = t->data; */
	
/* 	cbuf = cbuf_alloc(sz, &cb); */
/* 	memcpy(cbuf, reqs, sz); */
/* 	if (connection_parse_requests(c, cbuf, sz)) return -EINVAL; */
/* 	cbuf_free(cbuf); */

/* 	return sz; */
/* } */

/* int  */
/* content_read(spdid_t spdid, long connection_id, char *reqs, int sz) */
/* { */
/* 	struct torrent *t; */
/* 	struct connection *c; */
/* 	cbuf_t cb; */
/* 	char *cbuf; */
/* 	int ret; */
	
/* 	t = tor_lookup(connection_id); */
/* 	assert(t); */
/* 	c = t->data; */
	
/* 	cbuf = cbuf_alloc(sz, &cb); */
/* 	ret  = connection_get_reply(c, cbuf, sz); */
/* 	memcpy(reqs, cbuf, sz); */
/* 	cbuf_free(cbuf); */

/* 	return ret; */
/* } */

/* long */
/* content_create(spdid_t spdid, long evt_id, struct cos_array *d) */
/* { */
/* 	struct connection *c = http_new_connection(0, evt_id); */
/* 	struct torrent *t; */
	
/* 	t = tor_alloc(c, TOR_ALL); */
/* 	if (!t) return -1; */
/* 	c->conn_id = t->td; */
/* 	if (t->td < 0) { */
/* 		http_free_connection(c); */
/* 		return -ENOMEM; */
/* 	} */
/* 	return c->conn_id; */
/* } */

/* int  */
/* content_remove(spdid_t spdid, long conn_id) */
/* { */
/* 	struct torrent *t; */
/* 	struct connection *c; */

/* 	if (!tor_is_usrdef(conn_id)) return -1; */

/* 	t = tor_lookup(conn_id); */
/* 	if (!t) goto done; */
/* 	c = t->data; */
/* 	if (c) { */
/* 		http_free_connection(c); */
/* 		/\* bookkeeping *\/ */
/* 		http_conn_cnt++; */
/* 	} */
/* 	tor_free(t); */
/* done: */
/* 	return 0; */
/* } */

#define HTTP_REPORT_FREQ 100

void cos_init(void *arg)
{
	torlib_init();
	lock_static_init(&h_lock);

	if (periodic_wake_create(cos_spd_id(), HTTP_REPORT_FREQ)) BUG();
	while (1) {
		periodic_wake_wait(cos_spd_id());
		printc("HTTP conns %ld, reqs %ld\n", http_conn_cnt, http_req_cnt);
		http_conn_cnt = http_req_cnt = 0;
	}
	
	return;
}
