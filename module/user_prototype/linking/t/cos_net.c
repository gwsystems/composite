/**
 * Copyright 2008 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Initial author:  Gabriel Parmer, gabep1@cs.bu.edu, 2008
 */

/* 
 *  This is a little bit of a mess, but it is a mess with motivation:
 *  The data-structures employed include ring buffers of memory
 *  contained in pages (such that no span of MTU length crosses page
 *  boundries -- motivation being 1) so that the kernel can map and
 *  access this memory easily, 2) so that it can contain user-buffers
 *  also existing in user-components).  Thus exist the buff_page and
 *  rb_meta_t.  The thd_map maps between an upcall id and an
 *  associated ring buffer.  When an upcall is activated it can look
 *  up this mapping to find which ring buffer it should read from. A
 *  thd_map should really be simpler (not a struct, just a simple
 *  small array), but the refactoring to do this needs to be done in
 *  the future.  Next, we need a mechanism to keep track of specific
 *  connections, and how they are associated to threads so that we can
 *  provide (e.g. packet queues for received UDP data) and some sort
 *  of end-point for threads to block/wake on.  This is the struct
 *  intern_connection.  An opaque handle to these connections is
 *  net_connection_t.  There are mapping functions to convert between
 *  the two.  The packet queues in the intern_connections are
 *  implemented in an ugly, but easy and efficient way: the queue is
 *  implemented as a singly linked list, with a per-packet length, and
 *  pointer to the data. This struct packet_queue is actually not
 *  allocated separately, and exists in the ip_hdr region (after all
 *  packet processing is done).  Next there need to be exported
 *  functions for other components to use these mechanisms.  Herein
 *  lies the net_* interface.
 */
#define COS_FMT_PRINT 

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#include <cos_list.h>
#include <cos_map.h>
#include <cos_synchronization.h>
#include <cos_net.h>

#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/udp.h>
#include <lwip/tcp.h>
#include <lwip/stats.h>

#include <string.h>
#include <errno.h>

#define NUM_THDS MAX_NUM_THREADS
#define NUM_WILDCARD_BUFFS 256 //64 //32
#define UDP_RCV_MAX (1<<15)
/* 
 * We need page-aligned data for the network ring buffers.  This
 * structure lies at the beginning of a page and describes the data in
 * it.  When amnt_buffs = 0, we can dealloc the page.
 */
#define NP_NUM_BUFFS 2
#define MTU 1500
#define MAX_SEND 1400
#define BUFF_ALIGN_VALUE 8
#define BUFF_ALIGN(sz)  ((sz + BUFF_ALIGN_VALUE) & ~(BUFF_ALIGN_VALUE-1))
struct buff_page {
	int amnt_buffs;
	struct buff_page *next, *prev;
	void *buffs[NP_NUM_BUFFS];
	short int buff_len[NP_NUM_BUFFS];
	/* FIXME: should be a bit-field */
	char buff_used[NP_NUM_BUFFS];
};

/* Meta-data for the circular queues */
typedef struct {
	/* pointers within the buffer, counts of buffers within the
	 * ring, and amount of total buffers used for a given
	 * principal both in the driver ring, and in the stack. */
	unsigned int rb_head, rb_tail, curr_buffs, max_buffs, tot_principal, max_principal;
	ring_buff_t *rb;
	cos_lock_t l;
	struct buff_page used_pages, avail_pages;
} rb_meta_t;
static rb_meta_t rb1_md_wildcard, rb2_md;//, rb3_md;
static ring_buff_t rb1, rb2;//, rb3;
unsigned short int wildcard_upcall = 0;

//cos_lock_t tmap_lock;
struct thd_map {
	rb_meta_t *uc_rb;
} tmap[NUM_THDS];

extern int sched_component_take(spdid_t spdid);
extern int sched_component_release(spdid_t spdid);
extern unsigned long sched_timestamp(void);

extern int evt_trigger(spdid_t spdid, long extern_evt);
extern int evt_wait(spdid_t spdid, long extern_evt);
extern long evt_grp_wait(spdid_t spdid);
extern int evt_create(spdid_t spdid, long extern_evt);

cos_lock_t alloc_lock;
cos_lock_t net_lock;

#define NET_LOCK_TAKE()    \
	do {								\
		if (lock_take(&net_lock)) prints("error taking net lock."); \
	} while(0)

#define NET_LOCK_RELEASE() \
	do {								\
		if (lock_release(&net_lock)) prints("error releasing net lock."); \
	} while (0)

//#define NET_LOCK_TAKE()    sched_component_take(cos_spd_id())
//#define NET_LOCK_RELEASE() sched_component_release(cos_spd_id())

struct cos_net_xmit_headers xmit_headers;

/******************* Manipulations for the thread map: ********************/

/* static struct thd_map *get_thd_map_port(unsigned short port)  */
/* { */
/* 	int i; */

/* 	lock_take(&tmap_lock); */
/* 	for (i = 0 ; i < NUM_THDS ; i++) { */
/* 		if (tmap[i].port == port) { */
/* 			lock_release(&tmap_lock); */
/* 			return &tmap[i]; */
/* 		} */
/* 	} */
/* 	lock_release(&tmap_lock); */
	
/* 	return NULL; */
/* } */

static struct thd_map *get_thd_map(unsigned short int thd_id)
{
	assert(thd_id < NUM_THDS);

//	lock_take(&tmap_lock);
//		lock_release(&tmap_lock);
	return &tmap[thd_id];
//	lock_release(&tmap_lock);
	
	return NULL;
}

static int add_thd_map(unsigned short int ucid, /*unsigned short int port,*/ rb_meta_t *rbm)
{
	assert(ucid < NUM_THDS);
	
//	lock_take(&tmap_lock);
	tmap[ucid].uc_rb = rbm;
//	lock_release(&tmap_lock);

	return 0;
}

static int rem_thd_map(unsigned short int tid)
{
	struct thd_map *tm;

	/* Utilizing recursive locks here... */
//	lock_take(&tmap_lock);
	tm = get_thd_map(tid);
	if (!tm) {
//		lock_release(&tmap_lock);
		return -1;
	}
	tm->uc_rb = NULL;
//	lock_release(&tmap_lock);

	return 0;
}


/*********************** Ring buffer, and memory management: ***********************/

static void rb_init(rb_meta_t *rbm, ring_buff_t *rb)
{
	int i;

	for (i = 0 ; i < RB_SIZE ; i++) {
		rb->packets[i].status = RB_EMPTY;
	}
	memset(rbm, 0, sizeof(rb_meta_t));
	rbm->rb_head       = 0;
	rbm->rb_tail       = RB_SIZE-1;
	rbm->rb            = rb;
//	rbm->curr_buffs    = rbm->max_buffs     = 0; 
//	rbm->tot_principal = rbm->max_principal = 0;
	lock_static_init(&rbm->l);
	INIT_LIST(&rbm->used_pages, next, prev);
	INIT_LIST(&rbm->avail_pages, next, prev);
}

static int rb_add_buff(rb_meta_t *r, void *buf, int len)
{
	ring_buff_t *rb = r->rb;
	unsigned int head;
	struct rb_buff_t *rbb;

	assert(rb);
	lock_take(&r->l);
	head = r->rb_head;
	assert(head < RB_SIZE);
	rbb = &rb->packets[head];

	/* Buffer's full! */
	if (head == r->rb_tail) {
		goto err;
	}
	assert(rbb->status == RB_EMPTY);
	rbb->ptr = buf;
	rbb->len = len;

//	print("Adding buffer %x to ring.%d%d", (unsigned int)buf, 0,0);
	/* 
	 * The status must be set last.  It is the manner of
	 * communication between the kernel and this component
	 * regarding which cells contain valid pointers. 
	 *
	 * FIXME: There should be a memory barrier here, but I'll
	 * cross my fingers...
	 */
	rbb->status = RB_READY;
	r->rb_head = (r->rb_head + 1) & (RB_SIZE-1);
	lock_release(&r->l);

	return 0;
err:
	lock_release(&r->l);
	return -1;
}

/* 
 * -1 : there is no available buffer
 * 1  : the kernel found an error with this buffer, still set address
 *      and len.  Either the address was not mapped into this component, 
 *      or the memory region did not fit into a page.
 * 0  : successful, address contains data
 */
static int rb_retrieve_buff(rb_meta_t *r, void **buf, int *max_len)
{
	ring_buff_t *rb;
	unsigned int tail;
	struct rb_buff_t *rbb;
	unsigned short int status;

	assert(r);
	lock_take(&r->l);
	rb = r->rb;
	assert(rb);
	assert(r->rb_tail < RB_SIZE);
	tail = (r->rb_tail + 1) & (RB_SIZE-1);
	assert(tail < RB_SIZE);
	/* Nothing to retrieve */
	if (/*r->rb_*/tail == r->rb_head) {
		goto err;
	}
	rbb = &rb->packets[tail];
	status = rbb->status;
	if (status != RB_USED && status != RB_ERR) {
		goto err;
	}
	
	*buf = rbb->ptr;
	*max_len = rbb->len;
	/* Again: the status must be set last.  See comment in rb_add_buff. */
	rbb->status = RB_EMPTY;
	r->rb_tail = tail;

	lock_release(&r->l);
	if (status == RB_ERR) return 1;
	return 0;
err:
	lock_release(&r->l);
	return -1;
}

static struct buff_page *alloc_buff_page(void)
{
	struct buff_page *page;
	int i;
	int buff_offset = BUFF_ALIGN(sizeof(struct buff_page));

	lock_take(&alloc_lock);

	page = alloc_page();
	if (!page) {
		lock_release(&alloc_lock);
		return NULL;
	}
	page->amnt_buffs = 0;
	INIT_LIST(page, next, prev);
	for (i = 0 ; i < NP_NUM_BUFFS ; i++) {
		char *bs = (char *)page;

		page->buffs[i] = bs + buff_offset;
		page->buff_used[i] = 0;
		page->buff_len[i] = MTU;
		buff_offset += MTU;
	}
	lock_release(&alloc_lock);
	return page;
}

static void *alloc_rb_buff(rb_meta_t *r)
{
	struct buff_page *p;
	int i;
	void *ret = NULL;

	lock_take(&r->l);
	if (EMPTY_LIST(&r->avail_pages, next, prev)) {
		if (NULL == (p = alloc_buff_page())) {
			lock_release(&r->l);
			return NULL;
		}
		ADD_LIST(&r->avail_pages, p, next, prev);
	}
	p = FIRST_LIST(&r->avail_pages, next, prev);
	assert(p->amnt_buffs < NP_NUM_BUFFS);
	for (i = 0 ; i < NP_NUM_BUFFS ; i++) {
		if (p->buff_used[i] == 0) {
			p->buff_used[i] = 1;
			ret = p->buffs[i];
			p->amnt_buffs++;
			break;
		}
	}
	assert(NULL != ret);
	if (p->amnt_buffs == NP_NUM_BUFFS) {
		REM_LIST(p, next, prev);
		ADD_LIST(&r->used_pages, p, next, prev);
	}
	lock_release(&r->l);
	return ret;
}

static void release_rb_buff(rb_meta_t *r, void *b)
{
	struct buff_page *p;
	int i;

	assert(r && b);

	p = (struct buff_page *)(((unsigned long)b) & ~(4096-1));

	lock_take(&r->l);
	for (i = 0 ; i < NP_NUM_BUFFS ; i++) {
		if (p->buffs[i] == b) {
			p->buff_used[i] = 0;
			p->amnt_buffs--;
			REM_LIST(p, next, prev);
			ADD_LIST(&r->avail_pages, p, next, prev);
			lock_release(&r->l);
			return;
		}
	}
	/* b must be malformed such that p (the page descriptor) is
	 * not at the start of its page */
	assert(0);
}

extern int sched_create_net_upcall(spdid_t spdid, unsigned short int port, int prio_delta);
extern int sched_block(spdid_t spdid);
extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id);

static unsigned short int cos_net_create_net_upcall(unsigned short int port, rb_meta_t *rbm)
{
	unsigned short int ucid;
	
	ucid = sched_create_net_upcall(cos_spd_id(), port, 1);
	if (cos_buff_mgmt(COS_BM_RECV_RING, rb1.packets, sizeof(rb1.packets), ucid)) {
		prints("net: could not setup recv ring.");
		return 0;
	}
	return ucid;
}


/*********************** Component Interface ************************/

typedef enum {
	FREE,
	UDP,
	TCP,
	TCP_CLOSED
} conn_t;

typedef enum {
	ACTIVE,
	RECVING,
	CONNECTING,
	ACCEPTING,
	SENDING
} conn_thd_t;

//#define TEST_TIMING

#ifdef TEST_TIMING

typedef enum {
	RECV,
	APP_RECV,
	APP_PROC,
	SEND,
	UPCALL_PROC,
	TIMING_MAX
} timing_t;

struct timing_struct {
	char *name;
	unsigned int cnt;
	unsigned long long tot, max, min;
};

#define TIMING_STRUCT_INIT(str)					\
	{.name = str, .cnt = 0, .tot = 0, .max = 0, .min = ~0}

struct timing_struct timing_records[] = {
	TIMING_STRUCT_INIT("tcp_recv"),
	TIMING_STRUCT_INIT("tcp_app_recv"),
	TIMING_STRUCT_INIT("app_proc"),
	TIMING_STRUCT_INIT("tcp_send"),
	TIMING_STRUCT_INIT("upcall_proc")
};

static unsigned long long timing_timestamp(void) {
	unsigned long long ts;

	rdtscll(ts);
	return ts;
}
static unsigned long long timing_record(timing_t type, unsigned long long prev)
{
	unsigned long long curr, elapsed;
	struct timing_struct *ts = &timing_records[type];

	rdtscll(curr);
	elapsed = curr - prev;
	ts->cnt++;
	ts->tot += elapsed;
	if (elapsed > ts->max) ts->max = elapsed;
	if (elapsed < ts->min) ts->min = elapsed;
	
	return curr;
}

static void timing_output(void)
{
	int i;

	for (i = 0 ; i < TIMING_MAX ; i++) {
		struct timing_struct *ts = &timing_records[i];

		if (ts->cnt > 0) {
			printc("%s: @ %ld, avg %lld, max %lld, min %lld (cnt %d)", 
			       ts->name, sched_timestamp(), ts->tot/(unsigned long long)ts->cnt, 
			       ts->max, ts->min, ts->cnt);
			ts->cnt = 0;
			ts->tot = 0;
			ts->max = 0;
			ts->min = ~0;
		}
	}
}
#endif

/* This structure will alias with the ip header */
struct packet_queue {
	struct packet_queue *next;
	void *data, *headers;
	u32_t len;
#ifdef TEST_TIMING
	/* Time stamps */
	unsigned long long ts_start; 
#endif
};

struct intern_connection {
	u16_t tid;
	spdid_t spdid;
	conn_t conn_type;
	net_connection_t connection_id;
	conn_thd_t thd_status;
	long data;
	union {
		struct udp_pcb *up;
		struct tcp_pcb *tp;
	} conn;
	struct thd_map *tm;

	/* FIXME: should include information for each packet
	 * concerning the source ip and port as it can vary for
	 * UDP. */
	struct packet_queue *incoming, *incoming_last;
	/* The size of the queue, and the offset into the current
	 * packet where the read pointer is. */
	int incoming_size, incoming_offset;

	/* Accept will create a connection.  A list of connections is
	 * stored here using the next pointer. */
	struct intern_connection *accepted_ic, *accepted_last;

	struct intern_connection *next;
#ifdef TEST_TIMING
	/* Time stamps */
	unsigned long long ts_start; 
#endif
};

COS_MAP_CREATE_STATIC(connections);

static int net_conn_init(void)
{
	cos_map_init_static(&connections);

	return 0;
}

/* Return the opaque connection value exported to other components for
 * a given internal_connection */
static inline net_connection_t net_conn_get_opaque(struct intern_connection *ic)
{
	return ic->connection_id;
}

/* Get the internal representation of a connection */
static inline struct intern_connection *net_conn_get_internal(net_connection_t nc)
{
	struct intern_connection *ic;

	ic = cos_map_lookup(&connections, nc);
	return ic;
}

static inline int net_conn_valid(net_connection_t nc)
{
	return 1;
}

static inline struct intern_connection *net_conn_alloc(conn_t conn_type, u16_t tid, long data)
{
	struct intern_connection *ic;
	net_connection_t nc;

	ic = malloc(sizeof(struct intern_connection));
	if (NULL == ic) return NULL;
	nc = cos_map_add(&connections, ic);
	if (-1 == nc) {
		free(ic);
		return NULL;
	}
	memset(ic, 0, sizeof(struct intern_connection));

	ic->connection_id = nc;
	ic->tid = tid;
	ic->thd_status = ACTIVE;
	ic->conn_type = conn_type;
	ic->data = data;

	return ic;
}

static inline void net_conn_free(struct intern_connection *ic)
{
	assert(ic);
	assert(0 == ic->incoming_size);

	cos_map_del(&connections, net_conn_get_opaque(ic));
	free(ic);

	return;
}

/* 
 * Packets received from the network are encoded to have a
 * packet_queue structure at their head.  lwip has no knowledge of
 * this, and it just passes around a void * to the packet's data.  To
 * convert between the packet_queue,data and data, we have these two
 * functions.  We must always free the packet queue version.
 */
static inline void *net_packet_data(struct packet_queue *p)
{
	return &p[1];
}

static inline struct packet_queue *net_packet_pq(void *data)
{
	return &(((struct packet_queue*)data)[-1]);
}

static void net_conn_free_packet_data(struct intern_connection *ic)
{
	struct packet_queue *pq, *pq_next;

	assert(ic->conn_type == TCP_CLOSED);
	pq = ic->incoming;
	while (pq) {
		pq_next = pq->next;
		ic->incoming_size -= pq->len;
		free(pq);
		pq = pq_next;
	}
	assert(ic->incoming_size == 0);
	/* FIXME: go through the accept queue closing those tcp
	 * connections too */
}

/* 
 * The pbuf->payload might point to the actual data, but we might want
 * to free the data, which means we want to find the real start of the
 * data. Here we assume that p->payload does point to the start of the
 * data, not a header.  Also assume that no IP options are used. 
 */
static inline void *cos_net_header_start(struct pbuf *p, conn_t ct)
{
	char *data = (char *)p->alloc_track;

	assert(data);
	return data;
}

/**** COS UDP function ****/

/* 
 * Receive packet from the lwip layer and place it in a buffer to the
 * cos udp layer.  Here we'll deallocate the pbuf, and use a custom
 * structure encoded in the packet headers to keep the queue of data
 * to be read by clients.
 */
static void cos_net_lwip_udp_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
				   struct ip_addr *ip, u16_t port)
{
	struct intern_connection *ic;
	struct packet_queue *pq, *last;
	void *headers;

	/* We should not receive a list of packets unless it is from
	 * this host to this host (then the headers will be another
	 * packet), but we aren't currently supporting this case. */
	assert(1 == p->ref);
	assert(NULL == p->next && p->tot_len == p->len);
	assert(p->len > 0);
	ic = (struct intern_connection*)arg;
	assert(UDP == ic->conn_type);

	headers = cos_net_header_start(p, UDP);
	assert (NULL != headers);
	/* Over our allocation??? */
	if (ic->incoming_size >= UDP_RCV_MAX) {
		assert(ic->thd_status != RECVING);
		assert(p->type == PBUF_ROM);
		//free(net_packet_pq(headers));
		assert(p->ref > 0);
		pbuf_free(p);

		return;
	}
	pq = net_packet_pq(headers);
	pq->data = p->payload;
	pq->len = p->len;
	pq->next = NULL;
	
	assert((NULL == ic->incoming) == (NULL == ic->incoming_last));
	/* Is the queue empty? */
	if (NULL == ic->incoming) {
		assert(NULL == ic->incoming_last);
		ic->incoming = ic->incoming_last = pq;
	} else {
		last = ic->incoming_last;
		last->next = pq;
		ic->incoming_last = pq;
	}
	ic->incoming_size += p->len;
	assert(1 == p->ref);
	p->payload = p->alloc_track = NULL;
	pbuf_free(p);

	/* If the thread blocked waiting for a packet, wake it up */
	if (RECVING == ic->thd_status) {
		ic->thd_status = ACTIVE;
		assert(ic->thd_status == ACTIVE); /* Detect races */
		sched_wakeup(cos_spd_id(), ic->tid);
	}

	return;
}

/* 
 * FIXME: currently we associate a connection with a thread (and an
 * upcall thread), which is restrictive.
 */
net_connection_t net_create_udp_connection(spdid_t spdid, long evt_id)
{
	struct udp_pcb *up;
	struct intern_connection *ic;
	struct thd_map *tm;
	net_connection_t ret;

	up = udp_new();	
	if (NULL == up) {
		prints("Could not allocate udp connection");
		ret = -ENOMEM;
		goto err;
	}
	ic = net_conn_alloc(UDP, cos_get_thd_id(), evt_id);
	if (NULL == ic) {
		prints("Could not allocate internal connection");
		ret = -ENOMEM;
		goto udp_err;
	}
	tm = get_thd_map(wildcard_upcall);
	if (NULL == tm) {
		ret = -EPERM;
		goto conn_err;
	}
	ic->spdid = spdid;
	ic->tm = tm;
	ic->conn.up = up;
	udp_recv(up, cos_net_lwip_udp_recv, (void*)ic);
	return net_conn_get_opaque(ic);

conn_err:
	net_conn_free(ic);
udp_err:
	udp_remove(up);
err:
	return ret;
}

static int cos_net_udp_recv(struct intern_connection *ic, void *data, int sz)
{
	int xfer_amnt = 0;

	/* If there is data available, get it */
	if (ic->incoming_size > 0) {
		struct packet_queue *pq;
		char *data_start;
		int data_left;

		pq = ic->incoming;
		assert(pq);
		data_start = ((char*)pq->data) + ic->incoming_offset;
		data_left = pq->len - ic->incoming_offset;
		assert(data_left > 0 && (u32_t)data_left <= pq->len);
		/* Consume all of first packet? */
		if (data_left <= sz) {
			ic->incoming = pq->next;
			if (ic->incoming_last == pq) {
				assert(NULL == ic->incoming);
				ic->incoming_last = NULL;
			}
			memcpy(data, data_start, data_left);
			xfer_amnt = data_left;
			ic->incoming_offset = 0;

			free(pq);
		} 
		/* Consume part of first packet */
		else {
			memcpy(data, data_start, sz);
			xfer_amnt = sz;
			ic->incoming_offset += sz;
		}
		ic->incoming_size -= xfer_amnt;
	}

	return xfer_amnt;
}

/**** COS TCP function ****/

/* 
 * This should be called every time that a tcp connection is closed.
 */
static void cos_net_lwip_tcp_err(void *arg, err_t err)
{
	struct intern_connection *ic = arg;
	assert(ic);

	switch(err) {
	case ERR_ABRT:
	case ERR_RST:
		assert(ic->conn_type == TCP);
		assert(ic->conn_type != TCP_CLOSED);
		if (-1 != ic->data && evt_trigger(cos_spd_id(), ic->data)) assert(0);
		ic->conn_type = TCP_CLOSED;
		ic->conn.tp = NULL;
		net_conn_free_packet_data(ic);
		break;
	default:
		printc("TCP error #%d: don't really have docs to know what this means.", err);
	}

	return;
}

static void __net_close(struct intern_connection *ic)
{
	switch (ic->conn_type) {
	case UDP:
	{
		struct udp_pcb *up;

		up = ic->conn.up;
		udp_remove(up);
		ic->conn.up = NULL;
		break;
	}
	case TCP:
	{
		struct tcp_pcb *tp;

//		prints("cosnet net_close: closing active TCP connection");
		tp = ic->conn.tp;
		assert(tp);
		tcp_abort(tp);
		assert(NULL == ic->conn.tp && ic->conn_type == TCP_CLOSED);
		break;
	}
	case TCP_CLOSED:
//		prints("cosnet net_close: finishing close of inactive TCP connection");
		assert(NULL == ic->conn.tp);
		break;
	default:
		assert(0);
	}
	net_conn_free(ic);
}

static err_t cos_net_lwip_tcp_recv(void *arg, struct tcp_pcb *tp, struct pbuf *p, err_t err)
{
	struct intern_connection *ic;
	struct packet_queue *pq, *last;
	void *headers;
	struct pbuf *first;
	
	ic = (struct intern_connection*)arg;
	assert(NULL != ic);
	assert(TCP == ic->conn_type);
	if (NULL == p) {
		assert(ic->conn.tp == tp);
		/* 
		 * This should call our registered error function
		 * above with ERR_ABRT, which will make progress
		 * towards closing the connection.
		 *
		 * Later, when the app calls some function in the API,
		 * TCP_CLOSED will be seen and the internal connection
		 * will be deallocated, and the application notified.
		 */
		tcp_abort(tp);
		assert(ic->conn_type == TCP_CLOSED && NULL == ic->conn.tp);
		return ERR_CLSD;
	}
	first = p;
	while (p) {
		struct pbuf *q;

		if (p->ref != 1) printc("pbuf with len %d, totlen %d and refcnt %d", p->len, p->tot_len, p->ref);
		assert(p->len > 0);
		assert(p->type == PBUF_ROM || p->type == PBUF_REF);
		headers = cos_net_header_start(p, TCP);
		assert (NULL != headers);
		pq = net_packet_pq(headers);
		pq->data = p->payload;
		pq->len = p->len;
		pq->next = NULL;
#ifdef TEST_TIMING
		pq->ts_start = timing_record(RECV, pq->ts_start);
#endif
	
		assert((NULL == ic->incoming) == (NULL == ic->incoming_last));
		/* Is the queue empty? */
		if (NULL == ic->incoming) {
			assert(NULL == ic->incoming_last);
			ic->incoming = ic->incoming_last = pq;
		} else {
			last = ic->incoming_last;
			last->next = pq;
			ic->incoming_last = pq;
		}
		ic->incoming_size += p->len;
		//assert(1 == p->ref);
		q = p->next;
		p->payload = p->alloc_track = NULL;
		if (NULL == q) assert(p->len == p->tot_len);
		assert(p->ref == 1);
		p = q;
	}
	/* Just make sure lwip is doing what we think its doing */
	assert(first->ref == 1);
	/* This should deallocate the entire chain */
	pbuf_free(first);

	if (-1 != ic->data && evt_trigger(cos_spd_id(), ic->data)) assert(0);
/* 	/\* If the thread blocked waiting for a packet, wake it up *\/ */
/* 	if (RECVING == ic->thd_status) { */
/* 		ic->thd_status = ACTIVE; */
/* 		assert(ic->thd_status == ACTIVE); /\* Detect races *\/ */
/* 		if (sched_wakeup(cos_spd_id(), ic->tid)) assert(0); */
/* 	} */

	return ERR_OK;
}

static err_t cos_net_lwip_tcp_sent(void *arg, struct tcp_pcb *tp, u16_t len)
{
	struct intern_connection *ic = arg;
	assert(ic);

	/* I don't know why this is happening, but even when sending
	 * nothing, it says that we send 1 byte on accepts.  There is
	 * no ic->data associated with the connection yet, so we have
	 * a problem. */
	if (-1 == ic->data) return ERR_OK;
	/* FIXME: fix sending so that we can block and everything will work. */
	//if (evt_trigger(cos_spd_id(), ic->data)) assert(0);

	return ERR_OK;
}

static err_t cos_net_lwip_tcp_connected(void *arg, struct tcp_pcb *tp, err_t err)
{
	struct intern_connection *ic = arg;
	assert(ic);
	
	assert(CONNECTING == ic->thd_status);
	if (sched_wakeup(cos_spd_id(), ic->tid)) assert(0);
	ic->thd_status = ACTIVE;
	prints("cos connected!");

	return ERR_OK;
}

static err_t cos_net_lwip_tcp_accept(void *arg, struct tcp_pcb *new_tp, err_t err);

/* FIXME: same as for the udp version of this function. */
static net_connection_t __net_create_tcp_connection(spdid_t spdid, u16_t tid, struct tcp_pcb *new_tp, long evt_id)
{
	struct tcp_pcb *tp;
	struct intern_connection *ic;
	struct thd_map *tm;
	net_connection_t ret;

	if (NULL == new_tp) {
		tp = tcp_new();	
		if (NULL == tp) {
			prints("Could not allocate tcp connection");
			ret = -ENOMEM;
			goto err;
		}
	} else {
		tp = new_tp;
	}
	ic = net_conn_alloc(TCP, tid, evt_id);
	if (NULL == ic) {
		prints("Could not allocate internal connection");
		ret = -ENOMEM;
		goto tcp_err;
	}
	tm = get_thd_map(wildcard_upcall);
	if (NULL == tm) {
		ret = -EPERM;
		goto conn_err;
	}
	ic->spdid = spdid;
	ic->tm = tm;
	ic->conn.tp = tp;
	tcp_arg(tp, (void*)ic);
	tcp_err(tp, cos_net_lwip_tcp_err);
	tcp_recv(tp, cos_net_lwip_tcp_recv);
	tcp_sent(tp, cos_net_lwip_tcp_sent);
	tcp_accept(tp, cos_net_lwip_tcp_accept);
	return net_conn_get_opaque(ic);
conn_err:
	net_conn_free(ic);
tcp_err:
	tcp_abort(tp);
err:
	return ret;
}

net_connection_t net_create_tcp_connection(spdid_t spdid, u16_t tid, long evt_id)
{
	return __net_create_tcp_connection(spdid, tid, NULL, evt_id);
}

static err_t cos_net_lwip_tcp_accept(void *arg, struct tcp_pcb *new_tp, err_t err)
{
	struct intern_connection *ic = arg, *ica;
	net_connection_t nc;

	assert(ic);
	
	if (0 > (nc = __net_create_tcp_connection(ic->spdid, ic->tid, new_tp, -1))) assert(0);

	ica = net_conn_get_internal(nc);
	if (NULL == ica) assert(0);
	ic->next = NULL;
	if (NULL == ic->accepted_ic) {
		assert(NULL == ic->accepted_last);
		ic->accepted_ic = ica;
		ic->accepted_last = ica;
	} else {
		assert(NULL != ic->accepted_last);
		ic->accepted_last->next = ica;
		ic->accepted_last = ica;
	}
	assert(-1 != ic->data);
	if (evt_trigger(cos_spd_id(), ic->data)) assert(0);

	return ERR_OK;
}

static int cos_net_tcp_recv(struct intern_connection *ic, void *data, int sz)
{
	int xfer_amnt = 0;

	assert(ic->conn_type == TCP);
	/* If there is data available, get it */
	if (ic->incoming_size > 0) {
		struct packet_queue *pq;
		struct tcp_pcb *tp;
		char *data_start;
		int data_left;

		pq = ic->incoming;
		assert(pq);
		data_start = ((char*)pq->data) + ic->incoming_offset;
		data_left = pq->len - ic->incoming_offset;
		assert(data_left > 0 && (u32_t)data_left <= pq->len);
		/* Consume all of first packet? */
		if (data_left <= sz) {
			ic->incoming = pq->next;
			if (ic->incoming_last == pq) {
				assert(NULL == ic->incoming);
				ic->incoming_last = NULL;
			}
			memcpy(data, data_start, data_left);
			xfer_amnt = data_left;
			ic->incoming_offset = 0;
#ifdef TEST_TIMING
			ic->ts_start = timing_record(APP_RECV, pq->ts_start);
#endif			
			free(pq);
		} 
		/* Consume part of first packet */
		else {
			memcpy(data, data_start, sz);
			xfer_amnt = sz;
			ic->incoming_offset += sz;
			assert(ic->incoming_offset >= 0 && (u32_t)ic->incoming_offset < pq->len);
		}
		ic->incoming_size -= xfer_amnt;
		tp = ic->conn.tp;
		tcp_recved(tp, xfer_amnt);
	}

	return xfer_amnt;
}

/**** COS generic networking functions ****/

static struct intern_connection *net_verify_tcp_connection(net_connection_t nc, int *ret)
{
	struct intern_connection *ic;
	u16_t tid = cos_get_thd_id();

	if (!net_conn_valid(nc)) {
		*ret = -EINVAL;
		goto done;
	}
	ic = net_conn_get_internal(nc);
	if (NULL == ic) {
		*ret = -EINVAL;
		goto done;
	}
	if (tid != ic->tid) {
		*ret = -EPERM;
		goto done;
	}
	assert(ACTIVE == ic->thd_status);
	if (TCP != ic->conn_type) {
		*ret = -ENOTSUP;
		goto done;
	}
	/* socket has not been bound */
	if (0 == ic->conn.tp->local_port) {
		*ret = -1;
		goto done;
	}
	return ic;
done:
	return NULL;
}

net_connection_t net_accept(spdid_t spdid, net_connection_t nc)
{
	struct intern_connection *ic, *new_ic;
	net_connection_t ret = -1;

	NET_LOCK_TAKE();
	ic = net_verify_tcp_connection(nc, &ret);
	if (NULL == ic) {
		ret = -EINVAL;
		goto done;
	}

	/* No accepts are pending on this connection?: block */
	if (NULL == ic->accepted_ic) {
		ret = -EAGAIN;
		goto done;
/* 		assert(ic->tid == cos_get_thd_id()); */
/* 		ic->thd_status = ACCEPTING; */
/* 		NET_LOCK_RELEASE(); */
/* 		prints("net_accept: blocking!"); */
/* 		if (sched_block(cos_spd_id()) < 0) assert(0); */
/* 		NET_LOCK_TAKE(); */
/* 		assert(ACTIVE == ic->thd_status); */
	}

	assert(NULL != ic->accepted_ic && NULL != ic->accepted_last);
	new_ic = ic->accepted_ic;
	ic->accepted_ic = new_ic->next;
	if (NULL == ic->accepted_ic) ic->accepted_last = NULL;
	new_ic->next = NULL;
	assert(ic->conn.tp);
	tcp_accepted(ic->conn.tp);
	ret = net_conn_get_opaque(new_ic);

done:
	NET_LOCK_RELEASE();
	return ret;
}

/* 
 * After a connection has been accepted, we need to associate it with
 * its "event" data, or the scalar to pass to the event component.
 * That is what this call does.
 */
int net_accept_data(spdid_t spdid, net_connection_t nc, long data)
{
	struct intern_connection *ic;
	int ret;

	NET_LOCK_TAKE();
	ic = net_verify_tcp_connection(nc, &ret);
	if (NULL == ic || -1 != ic->data) goto err;
	ic->data = data;
	/* If data has already arrived, but couldn't trigger the event
	 * because ->data was not set, trigger the event now. */
	if (0 < ic->incoming_size) evt_trigger(cos_spd_id(), data);
	NET_LOCK_RELEASE();

	return 0;	
err:
	NET_LOCK_RELEASE();
	return -1;
}

int net_listen(spdid_t spdid, net_connection_t nc, int queue)
{
	struct tcp_pcb *tp, *new_tp;
	struct intern_connection *ic;
	int ret = 0;
	spdid_t si;

	NET_LOCK_TAKE();
	ic = net_verify_tcp_connection(nc, &ret);
	if (NULL == ic) {
		ret = -EINVAL;
		goto done;
	}
	tp = ic->conn.tp;
	si = ic->spdid;
	assert(NULL != tp);
	new_tp = tcp_listen_with_backlog(tp, queue);
	if (NULL == new_tp) {
		ret = -ENOMEM;
		goto done;
	}
	ic->conn.tp = new_tp;
	tcp_arg(new_tp, ic);
	tcp_accept(new_tp, cos_net_lwip_tcp_accept);
	//if (0 > __net_create_tcp_connection(si, new_tp)) assert(0);
done:
	NET_LOCK_RELEASE();
	return ret;
}

static int __net_bind(spdid_t spdid, net_connection_t nc, struct ip_addr *ip, u16_t port)
{
	struct intern_connection *ic;
	u16_t tid = cos_get_thd_id();
	int ret = 0;

	NET_LOCK_TAKE();
	if (!net_conn_valid(nc)) {
		ret = -EINVAL;
		goto done;
	}
	ic = net_conn_get_internal(nc);
	if (NULL == ic) {
		ret = -EINVAL;
		goto done;
	}
	if (tid != ic->tid) {
		ret = -EPERM;
		goto done;
	}
	assert(ACTIVE == ic->thd_status);

	switch (ic->conn_type) {
	case UDP:
	{
		struct udp_pcb *up;

		up = ic->conn.up;
		assert(up);
		if (ERR_OK != udp_bind(up, ip, port)) {
			ret = -EPERM;
			goto done;
		}
		break;
	}
	case TCP:
	{
		struct tcp_pcb *tp;
		
		tp = ic->conn.tp;
		assert(tp);
		if (ERR_OK != tcp_bind(tp, ip, port)) {
			ret = -ENOMEM;
			goto done;
		}
		NET_LOCK_RELEASE();
		return 0;
	}
	case TCP_CLOSED:
//		__net_close(ic);
		ret = -EPIPE;
		break;
	default:
		assert(0);
	}

done:
	NET_LOCK_RELEASE();
	return ret;
}

int net_bind(spdid_t spdid, net_connection_t nc, u32_t ip, u16_t port)
{
	struct ip_addr ipa = *(struct ip_addr*)&ip;
	return __net_bind(spdid, nc, &ipa, port);
}

static int __net_connect(spdid_t spdid, net_connection_t nc, struct ip_addr *ip, u16_t port)
{
	struct intern_connection *ic;
	u16_t tid = cos_get_thd_id();
	
	NET_LOCK_TAKE();
	if (!net_conn_valid(nc)) goto perm_err;
	ic = net_conn_get_internal(nc);
	if (NULL == ic) goto perm_err;
	if (tid != ic->tid) goto perm_err;
	assert(ACTIVE == ic->thd_status);

	switch (ic->conn_type) {
	case UDP:
	{
		struct udp_pcb *up;

		up = ic->conn.up;
		if (ERR_OK != udp_connect(up, ip, port)) {
			NET_LOCK_RELEASE();
			return -EISCONN;
		}
		break;
	}
	case TCP:
	{
		struct tcp_pcb *tp;

		tp = ic->conn.tp;
		ic->thd_status = CONNECTING;
		if (ERR_OK != tcp_connect(tp, ip, port, cos_net_lwip_tcp_connected)) {
			ic->thd_status = ACTIVE;
			NET_LOCK_RELEASE();
			return -ENOMEM;
		}
		NET_LOCK_RELEASE();
		if (sched_block(cos_spd_id()) < 0) assert(0);
		assert(ACTIVE == ic->thd_status);
		/* When we wake up, we should be connected. */
		return 0;
	}
	case TCP_CLOSED:
//		__net_close(ic);
		NET_LOCK_RELEASE();
		return -EPIPE;
	default:
		assert(0);
	}
	NET_LOCK_RELEASE();
	return 0;
perm_err:
	NET_LOCK_RELEASE();
	return -EPERM;
}

int net_connect(spdid_t spdid, net_connection_t nc, u32_t ip, u16_t port)
{
	struct ip_addr ipa = *(struct ip_addr*)&ip;
	return __net_connect(spdid, nc, &ipa, port);
}

int net_close(spdid_t spdid, net_connection_t nc)
{
	struct intern_connection *ic;
	u16_t tid = cos_get_thd_id();

	NET_LOCK_TAKE();
	if (!net_conn_valid(nc)) goto perm_err;
	ic = net_conn_get_internal(nc);
	if (NULL == ic) goto perm_err; /* should really be EINVAL */
	if (tid != ic->tid) goto perm_err;
	assert(ACTIVE == ic->thd_status);

	__net_close(ic);
	NET_LOCK_RELEASE();
	return 0;
perm_err:
	NET_LOCK_RELEASE();
	return -EPERM;
}

int net_recv(spdid_t spdid, net_connection_t nc, void *data, int sz)
{
//	struct udp_pcb *up;
	struct intern_connection *ic;
	u16_t tid = cos_get_thd_id();
	int xfer_amnt = 0;

//	if (!cos_argreg_buff_intern(data, sz)) return -EFAULT;
	if (!net_conn_valid(nc)) return -EINVAL;

	NET_LOCK_TAKE();
	ic = net_conn_get_internal(nc);
	if (NULL == ic) {
		NET_LOCK_RELEASE();
		return -EINVAL;
	}
	if (tid != ic->tid) {
		NET_LOCK_RELEASE();
		return -EPERM;
	}

	switch (ic->conn_type) {
	case UDP:
		xfer_amnt = cos_net_udp_recv(ic, data, sz);
		break;
	case TCP:
		xfer_amnt = cos_net_tcp_recv(ic, data, sz);
		break;
	case TCP_CLOSED:
//		__net_close(ic);
		xfer_amnt = -EPIPE;
		break;
	default:
		printc("net_recv: invalid connection type: %d", ic->conn_type);
		assert(0);
	}
	assert(xfer_amnt <= sz);
	NET_LOCK_RELEASE();
	return xfer_amnt;
}

int net_send(spdid_t spdid, net_connection_t nc, void *data, int sz)
{
	struct intern_connection *ic;
	u16_t tid = cos_get_thd_id();
	int ret = sz;

//	if (!cos_argreg_buff_intern(data, sz)) return -EFAULT;
	if (!net_conn_valid(nc)) return -EINVAL;
	if (sz > MAX_SEND) return -EMSGSIZE;

	NET_LOCK_TAKE();
	ic = net_conn_get_internal(nc);
	if (NULL == ic) {
		ret = -EINVAL;
		goto err;
	}
	if (tid != ic->tid) {
		ret = -EPERM;
		goto err;
	}

	switch (ic->conn_type) {
	case UDP:
	{
		struct udp_pcb *up;
		struct pbuf *p;

		/* There's no blocking in the UDP case, so this is simple */
		up = ic->conn.up;
		p = pbuf_alloc(PBUF_TRANSPORT, sz, PBUF_ROM);
		if (NULL == p) {
			ret = -ENOMEM;
			goto err;
		}
		p->payload = data;

		if (ERR_OK != udp_send(up, p)) {
			pbuf_free(p);
			/* IP/port must not be set */
			ret = -ENOTCONN;
			goto err;
		}
		pbuf_free(p);
		break;
	}
	case TCP:
	{
		struct tcp_pcb *tp;
#define TCP_SEND_COPY
#ifdef TCP_SEND_COPY
		void *d;
		struct packet_queue *pq;
#endif
		tp = ic->conn.tp;
		if (tcp_sndbuf(tp) < sz) { 
			ret = 0;
			break;
		}
#ifdef TCP_SEND_COPY
		pq = malloc(sizeof(struct packet_queue) + sz);
		if (unlikely(NULL == pq)) {
			ret = -ENOMEM;
			goto err;
		}
#ifdef TEST_TIMING
		pq->ts_start = timing_record(APP_PROC, ic->ts_start);
#endif
		pq->headers = NULL;
		d = net_packet_data(pq);
		memcpy(d, data, sz);
		if (ERR_OK != (ret = tcp_write(tp, d, sz, 0))) {
#else
		if (ERR_OK != (ret = tcp_write(tp, data, sz, TCP_WRITE_FLAG_COPY))) {
#endif
			free(pq);
			printc("tcp_write returned %d (sz %d, tcp_sndbuf %d, ERR_MEM: %d)", 
			       ret, sz, tcp_sndbuf(tp), ERR_MEM);
			assert(0);
		}
		/* No implementation of nagle's algorithm yet.  Send
		 * out the packet immediately if possible. */
		if (ERR_OK != (ret = tcp_output(tp))) {
			printc("tcp_output returned %d, ERR_MEM: %d", ret, ERR_MEM);
			assert(0);
		}
		ret = sz;

		break;
	}
	case TCP_CLOSED:
		ret = -EPIPE;
		break;
	default:
		assert(0);
	}
err:
	NET_LOCK_RELEASE();
	return ret;
}

/************************ LWIP integration: **************************/

struct ip_addr ip, mask, gw;
struct netif   cos_if;

struct udp_pcb *upcb_200, *upcb_out;
struct tcp_pcb *tpcb_200, *tpcb_accept, *tpcb_out;

static void cos_net_interrupt(void)
{
	unsigned short int ucid = cos_get_thd_id();
	void *buff, *d;
	int max_len, len;
	struct thd_map *tm;
	struct pbuf *p;//, *np;
	struct ip_hdr *ih;
	struct packet_queue *pq;
#ifdef TEST_TIMING
	unsigned long long ts;
#endif
	NET_LOCK_TAKE();
	tm = get_thd_map(ucid);
	assert(tm);
	if (rb_retrieve_buff(tm->uc_rb, &buff, &max_len)) {
		prints("net: could not retrieve buffer from ring.");
		NET_LOCK_RELEASE();
		return;
	}
	ih = (struct ip_hdr*)buff;
	if (unlikely(4 != IPH_V(ih))) goto err;
	len = ntohs(IPH_LEN(ih));
	if (unlikely(len > MTU)) {
		printc("len %d > %d", len, MTU);
		goto err;
	}
	p = pbuf_alloc(PBUF_IP, len, PBUF_ROM);
	
	if (unlikely(!p)) {
		prints("OOM in interrupt: allocation of pbuf failed.\n");
		goto err;
	}
	/* For now, we're going to do an additional copy.  Currently,
	 * packets should be small, so this shouldn't hurt that badly.
	 * This is done because 1) we are freeing the packet
	 * elsewhere, 2) we want to malloc some (small) packets to
	 * save space and free up the ring buffers, 3) it is difficult
	 * to know in (1) which deallocation method (free or return to
	 * ring buff) to use */
	pq = malloc(len + sizeof(struct packet_queue));
	if (unlikely(NULL == pq)) {
		printc("OOM in interrupt: allocation of packet data (%d bytes) failed.\n", len);
		pbuf_free(p);
		goto err;
	}
	pq->headers = d = net_packet_data(pq);
#ifdef TEST_TIMING
#ifdef TCP_SEND_COPY
	ts = pq->ts_start = timing_timestamp();
#endif	
#endif	
	memcpy(d, buff, len);
	p->payload = p->alloc_track = d;
	/* free packet in this call... */
	if (ERR_OK != cos_if.input(p, &cos_if)) {
		prints("net: failure in IP input.");
		pbuf_free(p);
		goto err;
	}

	/* OK, recycle the buffer. */
	if (rb_add_buff(tm->uc_rb, buff /*(char *)p->payload - (UDP_HLEN + IP_HLEN)*/, MTU)) {
		prints("net: could not add buffer to ring.");
	}

#ifdef TEST_TIMING
	timing_record(UPCALL_PROC, ts);
#endif
	NET_LOCK_RELEASE();

	return;
err:
	/* Recycle the buffer (essentially dropping packet)... */
	if (rb_add_buff(tm->uc_rb, buff, MTU)) {
		prints("net: OOM, and filed to add buffer.");
	}
	NET_LOCK_RELEASE();
	return;
}

static err_t cos_net_stack_link_send(struct netif *ni, struct pbuf *p)
{
	/* We don't do arp...this shouldn't be called */
	assert(0);
	return ERR_OK;
}

static err_t cos_net_stack_send(struct netif *ni, struct pbuf *p, struct ip_addr *ip)
{
	int i;

	/* assuming the net lock is taken here */

	/* We are requiring chained pbufs here, one for the header,
	 * one for the data.  First assert checks that we have > 1
	 * pbuf, second asserts we have 2 */
	assert(p && p->ref == 1);
	xmit_headers.len = 0;
	if (p->len <= sizeof(xmit_headers.headers)) {
		assert(p->type == PBUF_RAM);
		memcpy(&xmit_headers.headers, p->payload, p->len);
		xmit_headers.len = p->len;
		p = p->next;
	} 

	/* 
	 * Here we do 2 things: create a separate gather data entry
	 * for each pbuf, and separate the data in individual pbufs
	 * into separate gather entries if it crosses page boundaries.
	 */
	for (i = 0 ; p && i < XMIT_HEADERS_GATHER_LEN ; i++) {
		char *data = p->payload;
		struct gather_item *gi = &xmit_headers.gather_list[i];
		int len_on_page;

		assert(data && p->len < PAGE_SIZE);
		gi->data = data;
		gi->len  = p->len;
		len_on_page = (unsigned long)round_up_to_page(data) - (unsigned long)data;
		/* Data split across pages??? */
		if (len_on_page < p->len) {
			int len_on_second = p->len - len_on_page;

			if (XMIT_HEADERS_GATHER_LEN == i+1) goto segment_err;
			gi->len  = len_on_page;
			gi = gi+1;
			gi->data = data + len_on_page;
			gi->len  = len_on_second;
			i++;
		}
#ifdef TCP_SEND_COPY
		if ((p->type == PBUF_REF || p->type == PBUF_ROM)) {
			struct packet_queue *pq;
			pq = net_packet_pq(p->payload);
#ifdef TEST_TIMING
			timing_record(SEND, pq->ts_start);
#endif
		}
#endif
		assert(p->type != PBUF_POOL);
		assert(p->ref == 1);
		p = p->next;
	}
	if (unlikely(NULL != p)) goto segment_err;
	xmit_headers.gather_len = i;

	/* Send the collection of pbuf data on its way. */
	if (cos_buff_mgmt(COS_BM_XMIT, NULL, 0, 0)) {
		prints("net: could not xmit data.");
	}

	/* cannot deallocate packets here as we might need to
	 * retransmit them. */
done:
	return ERR_OK;
segment_err:
	printc("net: attempted to xmit too many segments");
	goto done;
}

/* 
 * Called when pbuf_free is invoked on a pbuf that was allocated with
 * PBUF_{ROM|REF}.  Free the ->alloc_track if it is non-NULL.
 */
static void lwip_free_payload(struct pbuf *p)
{
	struct packet_queue *pq;
	void *headers;

	assert(p);
	if (NULL == p->payload) return;
	if (cos_argreg_buff_intern(p->payload, p->len)) {
		p->payload = NULL;
		return;
	}
	/* assuming this will only happen with TCP data */
	headers = cos_net_header_start(p, TCP);
	assert (NULL != headers); /* we could just return NULL here */
	pq = net_packet_pq(headers);
	/* have we successfully extracted the packet_queue? */
	assert(pq->headers == NULL || pq->headers == headers);
	p->payload = NULL;
	free(pq);
}

/* UDP functions (additionally see cos_net_stack_udp_recv above) */

static struct udp_pcb *cos_net_create_outbound_udp_conn(u16_t local_port, u16_t remote_port, 
							struct ip_addr *ip, struct thd_map *tm)
{
	struct udp_pcb *up;

	assert(tm && remote_port && ip);
	up = udp_new();
	if (!up) return NULL;
	if (local_port) {
		if (ERR_OK != udp_bind(up, IP_ADDR_ANY, local_port)) {
			prints("net: could not create outbound udp connection (bind).");
			udp_remove(up);
		}
		udp_recv(up, cos_net_lwip_udp_recv, (void*)tm);
	}
	if (ERR_OK != udp_connect(up, ip, remote_port)) {
		prints("net: could not create outbound udp connection (connect).");
		udp_remove(up);
	}
	return up;
}

static struct udp_pcb *cos_net_create_inbound_udp_conn(u16_t local_port, struct thd_map *tm)
{
	struct udp_pcb *up;

	assert(tm);
 	up = udp_new();
	if (!up) return NULL;
	if (ERR_OK != udp_bind(up, IP_ADDR_ANY, local_port)) {
		prints("net: could not create inbound udp_conn.");
		udp_remove(up);
		return NULL;
	}
	udp_recv(up, cos_net_lwip_udp_recv, (void*)tm);
	return up;
}

/*** Initialization routines: ***/

static err_t cos_if_init(struct netif *ni)
{
	ni->name[0]    = 'c';
	ni->name[1]    = 'n';
	ni->mtu        = 1500;
	ni->output     = cos_net_stack_send;
	ni->linkoutput = cos_net_stack_link_send;

	return ERR_OK;
}

static void init_lwip(void)
{
//	struct ip_addr dest;

	lwip_init();
	tcp_mem_free(lwip_free_payload);

	IP4_ADDR(&ip, 10,0,2,8);
	IP4_ADDR(&gw, 10,0,1,1); //korma
	IP4_ADDR(&mask, 255,255,255,0);
	
	netif_add(&cos_if, &ip, &mask, &gw, NULL, cos_if_init, ip_input);
	netif_set_default(&cos_if);
	netif_set_up(&cos_if);

//	upcb_200 = cos_net_create_inbound_udp_conn(200, get_thd_map(cos_get_thd_id()));
//	IP4_ADDR(&dest, 10,0,1,6);
//	upcb_out = cos_net_create_outbound_udp_conn(0, 6000, &dest, get_thd_map(cos_get_thd_id()));

//	tpcb_200 = 
}

extern int timed_event_block(spdid_t spdid, unsigned int usecs);

static void test_tcp(void)
{
	net_connection_t nc, nc_new;
	char data[128];
	int ret, len;
	struct intern_connection *ic;

	nc = net_create_tcp_connection(cos_spd_id(), cos_get_thd_id(), 1);
	if (evt_create(cos_spd_id(), 1)) assert(0);
	ic = net_conn_get_internal(nc);
	assert(ic);
	ic->data = 1;
	if (nc) print("create udp connection error: %d %d%d", nc, 0,0);
	printc("tcp connection created: %d.", nc);
	ret = __net_bind(cos_spd_id(), nc, IP_ADDR_ANY, 200);
	printc("%d bound to port 200", nc);
	if (ret) print("Bind error: %d. %d%d", ret, 0, 0);
	net_listen(cos_spd_id(), nc, 10);
	printc("%d set to listen", nc);
	nc_new = -EAGAIN;
	while (-EAGAIN == nc) {
		nc_new = net_accept(cos_spd_id(), nc);
		if (-EAGAIN == nc_new) {
			assert(0);
		}
	}
	printc("%d returned from accept", nc_new);
	ic = net_conn_get_internal(nc_new);
	assert(ic);
	ic->data = 2;
	if (evt_create(cos_spd_id(), 2)) assert(0);

	printc("%d accepted and returned connection %d", nc, nc_new);
	if (nc_new < 0) assert(0);
	//IP4_ADDR(&dest, 10,0,1,5);
	
	while (1) {
		len = net_recv(cos_spd_id(), nc_new, data, 128);
		if (0 == len) {
			if (2 != (int)evt_grp_wait(cos_spd_id()/*, 2*/)) assert(0);
		}
		//net_send(cos_spd_id(), nc_new, data, len);
	}
}

static void test_udp(void)
{
	net_connection_t nc, nco;
	char data[128], len;
	int ret;
	struct ip_addr dest;

	nc = net_create_udp_connection(cos_spd_id(), -1);
	if (nc) print("create udp connection error: %d %d%d", nc, 0,0);
	ret = __net_bind(cos_spd_id(), nc, IP_ADDR_ANY, 200);
	if (ret) print("Bind error: %d. %d%d", ret, 0, 0);
	nco = net_create_udp_connection(cos_spd_id(), -1);
	IP4_ADDR(&dest, 10,0,1,5);
	__net_connect(cos_spd_id(), nco, &dest, 6000);
	
	while (1) {
		len = net_recv(cos_spd_id(), nc, data, 128);
		assert(len > 0);
		net_send(cos_spd_id(), nco, data, len);
	}
}

static void test_thd(void)
{
//	test_tcp();
////	test_udp();
	sched_block(cos_spd_id());
}

static int init(void) 
{
	unsigned short int ucid, i;
	void *b;
	int cnt = 0;
#ifdef LWIP_STATS
	int stats_cnt = 0;
#endif

//	extern int alloc_debug;
//	alloc_debug = 1;

	lock_static_init(&alloc_lock);
//	lock_static_init(&tmap_lock);
	lock_static_init(&net_lock);

	rb_init(&rb1_md_wildcard, &rb1);
	rb_init(&rb2_md, &rb2);
//	rb_init(&rb3_md, &rb3);

	net_conn_init();

	/* Setup the region from which headers will be transmitted. */
	if (cos_buff_mgmt(COS_BM_XMIT_REGION, &xmit_headers, sizeof(xmit_headers), 0)) {
		prints("net: error setting up xmit region.");
	}

	NET_LOCK_TAKE();
	/* Wildcard upcall */
	ucid = cos_net_create_net_upcall(0, &rb1_md_wildcard);
	if (ucid == 0) {
		NET_LOCK_RELEASE();
		return 0;
	}
	wildcard_upcall = ucid;
	add_thd_map(ucid, /*0 wildcard port ,*/ &rb1_md_wildcard);
	
	init_lwip();

	for (i = 0 ; i < NUM_WILDCARD_BUFFS ; i++) {
		if(!(b = alloc_rb_buff(&rb1_md_wildcard))) {
			prints("net: could not allocate the ring buffer.");
		}
		if(rb_add_buff(&rb1_md_wildcard, b, MTU)) {
			prints("net: could not populate the ring with buffer");
		}
	}

	NET_LOCK_RELEASE();
	/* Start the tcp timer */
	while (1) {
		/* Sleep for a quarter of seconds as prescribed by lwip */
		NET_LOCK_TAKE();

		if (++cnt == 4) {
#ifdef TEST_TIMING
			timing_output();
#endif
		}
#ifdef LWIP_STATS
		if (++stats_cnt == 20) {
			stats_cnt = 0;
			stats_display();
		}
#endif
		tcp_tmr();
		NET_LOCK_RELEASE();
		timed_event_block(cos_spd_id(), 25); /* expressed in ticks currently */
	}

	//sched_block();

	prints("net: Error -- returning from init!!!");
	assert(0);
	return 0;
}

void cos_init(void *arg)
{
	 static volatile int first = 1;

	if (first) {
		first = 0;
		init();
		assert(0);
	} else {
		test_thd();
		prints("net: not expecting more than one bootstrap.");
	}
}

void cos_upcall_exec(void *arg)
{
	cos_net_interrupt();
}
