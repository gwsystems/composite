/**
 * Copyright 2009 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author:  Gabriel Parmer, gabep1@cs.bu.edu, 2009
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
#include <string.h>

#include <http.h>

#include <content_mux.h>
#include <timed_blk.h>
#include <sched.h>

#endif	/* COS_LINUX_ENV */

/* Keeping some stats (unsynchronized across threads currently) */
static volatile unsigned long http_conn_cnt = 0, http_req_cnt = 0;

inline static int is_whitespace(char s)
{
	if (' ' == s) return 1;
	return 0;
}

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

extern long content_open(spdid_t spdid, long evt_id, struct cos_array *data);
extern int content_request(spdid_t spdid, long cr, struct cos_array *data);
extern int content_retrieve(spdid_t spdid, long cr, struct cos_array *data, int *more);
extern int content_close(spdid_t spdid, long cr);

extern int timed_event_block(spdid_t spdid, unsigned int microsec);

struct http_response {
	char *resp;
	int resp_len;
	int more; 		/* is there more data to retrieve? */
};

struct connection {
	int refcnt;
	long conn_id, evt_id;
	struct http_request *pending_reqs;
};

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
	struct cos_array *arg;
	int ret = -1;
	assert(r && r->c);

	arg = cos_argreg_alloc(r->path_len + sizeof(struct cos_array) + 1);
	assert(arg);
	memcpy(arg->mem, r->path, r->path_len);
	arg->sz = r->path_len;
	arg->mem[arg->sz] = '\0';
	if (0 > r->content_id ) {
		r->content_id = content_open(cos_spd_id(), r->c->evt_id, arg);
		if (r->content_id < 0) {
			cos_argreg_free(arg);
			return r->content_id;
		}
	}
	ret = content_request(cos_spd_id(), r->content_id, arg);
	cos_argreg_free(arg);
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
	content_close(cos_spd_id(), r->content_id);
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
	"Date: Sat, 14 Feb 2008 14:59:00 GMT\r\n"
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
		struct cos_array *arr = NULL;
		char *local_resp;
		int *more = NULL; 
		int local_more, consumed, ret, local_resp_sz;

		assert(r->c == c);
		if (r->flags & HTTP_REQ_PENDING) break;
		assert(r->flags & HTTP_REQ_PROCESSED);
		assert(r->content_id >= 0);

		/* Previously saved response? */
		if (NULL != r->resp.resp) {
			local_resp = r->resp.resp;
			local_resp_sz = r->resp.resp_len;
			local_more = r->resp.more;
		} else {
			/* Make the request to the content
			 * component */
			more = cos_argreg_alloc(sizeof(int));
			assert(more);
			arr = cos_argreg_alloc(sizeof(struct cos_array) + resp_sz - used);
			assert(arr);

			arr->sz = resp_sz - used;
			if ((ret = content_retrieve(cos_spd_id(), r->content_id, arr, more))) {
				cos_argreg_free(arr);
				cos_argreg_free(more);
				if (0 > ret) {
					BUG();
					/* FIXME send an error message. */
				}
				printc("https get reply returning %d.\n", ret);
				return ret;
			}
			local_more = *more;
			local_resp_sz = arr->sz;
			local_resp = arr->mem;
		}
		
		/* still more date, but not available now... */
		if (local_resp_sz == 0) {
			cos_argreg_free(arr);
			cos_argreg_free(more);
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
				assert(arr);
				memcpy(save, arr->mem, local_resp_sz);
				cos_argreg_free(arr);
				r->resp.more = *more;
				cos_argreg_free(more);

				r->resp.resp = save;
				r->resp.resp_len = local_resp_sz;
			}
			if (0 == used) {
				printc("https: could not allocate either header or response of sz %d:%s\n", local_resp_sz, local_resp);
				if (arr) cos_argreg_free(arr);
				if (more) cos_argreg_free(more);
				return -ENOMEM;
			}
			break;
		}

		memcpy(resp+used+consumed, local_resp, local_resp_sz);
		
		if (arr)  cos_argreg_free(arr);
		if (more) cos_argreg_free(more);
		more = NULL;
		arr  = NULL;

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

COS_MAP_CREATE_STATIC(conn_map);

static int connection_process_requests(struct connection *c, char *req, int req_sz,
				char *resp, int resp_sz)
{
	/* FIXME: close connection on error? */
	if (connection_parse_requests(c, req, req_sz)) return -EINVAL;
	return connection_get_reply(c, resp, resp_sz);
}

long content_split(spdid_t spdid, long conn_id, long evt_id)
{
	return -ENOSYS;
}

int content_write(spdid_t spdid, long connection_id, char *reqs, int sz)
{
	struct connection *c;

//	printc("HTTP write");
	
	c = cos_map_lookup(&conn_map, connection_id);
	if (NULL == c) return -EINVAL;
	if (connection_parse_requests(c, reqs, sz)) return -EINVAL;
	
	return sz;
}

int content_read(spdid_t spdid, long connection_id, char *buff, int sz)
{
	struct connection *c;
	
//	printc("HTTP read");

	c = cos_map_lookup(&conn_map, connection_id);
	if (NULL == c) return -EINVAL;
	
	return connection_get_reply(c, buff, sz);
}

static int http_read_write(spdid_t spdid, long connection_id, char *reqs, int req_sz, char *resp, int resp_sz)
{
	struct connection *c;
	
	c = cos_map_lookup(&conn_map, connection_id);
	if (NULL == c) return -EINVAL;

	return connection_process_requests(c, reqs, req_sz, resp, resp_sz);
}

long content_create(spdid_t spdid, long evt_id, struct cos_array *d)
{
	struct connection *c = http_new_connection(0, evt_id);
	long c_id;

//	printc("HTTP open connection");
	if (NULL == c) return -ENOMEM;
	c_id = cos_map_add(&conn_map, c);
	if (c_id < 0) {
		http_free_connection(c);
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
	http_free_connection(c);

	/* bookkeeping */
	http_conn_cnt++;

	return 0;
}

#define HTTP_REPORT_FREQ 100

void cos_init(void *arg)
{
	cos_map_init_static(&conn_map);

	while (1) {
		timed_event_block(cos_spd_id(), HTTP_REPORT_FREQ);
		printc("HTTP conns %ld, reqs %ld\n", http_conn_cnt, http_req_cnt);
		http_conn_cnt = http_req_cnt = 0;
	}
	
	return;
}

#ifdef COS_LINUX_ENV

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <unistd.h>


#define BUFF_SZ 256//1024

static int connection_event(struct connection *c)
{
	int amnt;
	char buff[BUFF_SZ+1];
	buff[BUFF_SZ] = '\0';

	amnt = read(c->evt_id, buff, BUFF_SZ);
	if (0 == amnt) return 1;
	if (amnt < 0) {
		printf("read from fd %ld, ret %d\n", c->evt_id, amnt);
		perror("read from session");
		return -1;
	}
	buff[amnt] = '\0';

	/* The work: */
	if (amnt != http_write(c->conn_id, buff, amnt)) {
		printf("Error writing.\n");
		return 1;
	}
	while (1) {
		if ((amnt = http_read(c->conn_id, buff, BUFF_SZ)) < 0) {
			printf("Error reading (%d)\n", amnt);
			return -1;
		}
		if (0 == amnt) break;
		if (write(c->evt_id, buff, amnt) != amnt) {
			perror("writing");
			return -1;
		}
	}

	return 0;
}

int connmgr_create_server(short int port)
{
	int fd;
	struct sockaddr_in server;

	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Establishing socket");
		return -1;
	}

	server.sin_family      = AF_INET;
	server.sin_port        = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(fd, (struct sockaddr *)&server, sizeof(server))) {
		perror("binding receive socket");
		return -1;
	}
	listen(fd, 10);

	return fd;
}

struct connection *connmgr_accept(int fd)
{
	struct sockaddr_in sai;
	int new_fd;
	unsigned int len = sizeof(sai);
	struct connection *c;
	long c_id;

	new_fd = accept(fd, (struct sockaddr *)&sai, &len);
	if (-1 == new_fd) {
		perror("accept");
		return NULL;
	}
	c_id = http_open_connection(new_fd);
	c = cos_map_lookup(&conn_map, c_id);
	if (NULL == c) http_close_connection(c_id);

	return c;
}

#define MAX_CONNECTIONS 100
struct epoll_event evts[MAX_CONNECTIONS];

static void event_new(int evt_fd, struct connection *c)
{
	struct epoll_event e;
	assert(c);

	e.events = EPOLLIN;//|EPOLLOUT;
	e.data.ptr = c;
	if (epoll_ctl(evt_fd, EPOLL_CTL_ADD, c->evt_id, &e)) {
		perror("epoll create event");
		exit(-1);
	}

	return;
}

static void event_delete(int evt_fd, struct connection *c)
{
	if (epoll_ctl(evt_fd, EPOLL_CTL_DEL, c->evt_id, NULL)) {
		perror("epoll delete event");
		exit(-1);
	}
}

#include <signal.h>
void prep_signals(void)
{
	struct sigaction sa;

	sa.sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL)) {
		perror("sigaction");
		exit(-1);
	}
}

int ut_num = 0;

enum {UT_SUCCESS, UT_FAIL, UT_PENDING};
static int print_ut(char *s, int len, get_ret_t *ret, int type, int print_success)
{
	int r;

	switch(type) {
	case UT_SUCCESS:
		r = http_get_parse(s, len, ret);
		break;
	case UT_FAIL:
		r = !http_get_parse(s, len, ret);
		break;
	case UT_PENDING:
		r = (!http_get_parse(s, len, ret) && ret->end != s+len);
		break;
	}

	if (r) {
		printf("******************\nUnit test %d failed:\n"
		       "String (@%p):\n%s\nResult(@%p):\n<%s>\n******************\n\n",
		       ut_num, s, s, ret->end, ret->end);
		ut_num++;
		return 1;
	} else if (print_success) {
		printf("Unit test %d successful(s@%p-%p, e@%p).\n", ut_num, s, s+len, ret->end);
	}
	ut_num++;
	return 0;
}

static int unittest_http_parse(int print_success)
{
	int success = 1, i;
	char *ut_successes[] = {
		"GET / HTTP/1.1\r\nUser-Agent: httperf/0.9.0\r\nHost: localhost\r\n\r\n",
		NULL
	};
	char *ut_pend[] = {
		"G",		/* 1 */
		"GET",
		"GET ",
		"GET /",
		"GET / ",	/* 5 */
		"GET / HT",
		"GET / HTTP/1.",
		"GET / HTTP/1.1",
		"GET / HTTP/1.1\r",
		"GET / HTTP/1.1\r\n", /* 10 */
		"GET / HTTP/1.1\r\nUser-",
		"GET / HTTP/1.1\r\nUser-blah:blah\r",
		NULL
	};
	char *ut_fail[] = {
		"GET / HTTP/1.2",
		"GET / HTTP/1.2\r\nUser-blah:blah\r", /* 14 */
		NULL
	};
	get_ret_t ret;

	for (i = 0 ; ut_successes[i] ; i++) {
		if (print_ut(ut_successes[i], strlen(ut_successes[i]), &ret, UT_SUCCESS, print_success)) {
			success = 0;
		}
	}
	for (i = 0 ; ut_pend[i] ; i++) {
		if (print_ut(ut_pend[i], strlen(ut_pend[i]), &ret, UT_PENDING, print_success)) {
			success = 0;
		}
	}
	for (i = 0 ; ut_fail[i] ; i++) {
		if (print_ut(ut_fail[i], strlen(ut_fail[i]), &ret, UT_FAIL, print_success)) {
			success = 0;
		}
	}

	if (success) return 0;
	return 1;
}

int main(void)
{
	int sfd, epfd;
	struct connection main_c;
	struct epoll_event new_evts[MAX_CONNECTIONS];

	if (unittest_http_parse(0)) return -1;

	prep_signals();

	epfd = epoll_create(MAX_CONNECTIONS);
	sfd = connmgr_create_server(8000);
	main_c.evt_id = sfd;
	event_new(epfd, &main_c);

	cos_init(NULL);

	while (1) {
		int nevts, i, accept_event = 0;

		nevts = epoll_wait(epfd, new_evts, MAX_CONNECTIONS, -1);
		if (nevts < 0) {
			perror("waiting for events");
			return -1;
		}
		for (i = 0 ; i < nevts ; i++) {
			struct epoll_event *e = &new_evts[i];
			struct connection *c = (struct connection *)e->data.ptr;

			if (c == &main_c) {
				if (e->events & (EPOLLERR | EPOLLHUP)) {
					printf("errors on the listen fd\n");
					return -1;
				}
				accept_event = 1;
			} else if (e->events & (EPOLLERR | EPOLLHUP)) {
				event_delete(epfd, c);
				close(c->evt_id);
				http_close_connection(c->conn_id);
				/* FIXME: free requests for connection */
			} else {
				int ret;

				ret = connection_event(c);
				if (ret > 0) {
					event_delete(epfd, c);
					close(c->evt_id);
					http_close_connection(c->conn_id);
				} else if (ret < 0) {
					return -1;
				}
			}
		}

		if (accept_event) {
			struct connection *c;
			c = connmgr_accept(main_c.evt_id);
			if (NULL == c) {
				printf("Not a large enough connection namespace.");
			} else {
				event_new(epfd, c);
			}
		}
	}
	return 0;
}

#endif
