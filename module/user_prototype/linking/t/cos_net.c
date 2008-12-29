/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
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
#include <cos_synchronization.h>
#include <cos_net.h>

#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/udp.h>
#include <lwip/tcp.h>

#include <string.h>
#include <errno.h>

#define NUM_THDS MAX_NUM_THREADS
#define NUM_WILDCARD_BUFFS 64 //32
#define UDP_RCV_MAX (1<<15)
/* 
 * We need page-aligned data for the network ring buffers.  This
 * structure lies at the beginning of a page and describes the data in
 * it.  When amnt_buffs = 0, we can dealloc the page.
 */
#define NP_NUM_BUFFS 2
#define MTU 1500
#define MAX_UDP_SEND 1400
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
	int rb_head, rb_tail, curr_buffs, max_buffs, tot_principal, max_principal;
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

cos_lock_t alloc_lock;
cos_lock_t     net_lock;

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

extern int sched_create_net_upcall(unsigned short int port, int depth);
extern int sched_block(spdid_t spdid);
extern int sched_wakeup(spdid_t spdid, unsigned short int thd_id);

static unsigned short int cos_net_create_net_upcall(unsigned short int port, rb_meta_t *rbm)
{
	unsigned short int ucid;
	
	ucid = sched_create_net_upcall(port, 1);
	if (cos_buff_mgmt(rb1.packets, sizeof(rb1.packets), ucid, COS_BM_RECV_RING)) {
		prints("net: could not setup recv ring.");
		return 0;
	}
	return ucid;
}


/*********************** Component Interface ************************/

#define MAX_CONNS 256

typedef enum {
	FREE,
	UDP,
	TCP
} conn_t;

typedef enum {
	ACTIVE,
	BLOCKED
} conn_thd_t;

/* This structure will alias with the ip header */
struct packet_queue {
	struct packet_queue *next;
	void *data;
	u32_t len;
};

struct intern_connection {
	u16_t tid;
	conn_t conn_type;
	conn_thd_t thd_status;
	union {
		struct udp_pcb *up;
		struct tcp_pcb *tp;
	} conn;
	struct thd_map *tm;

	/* UDP only.  TCP keeps its own queues we can use.  FIXME:
	 * should include information for each packet concerning the
	 * source ip and port as it can vary. */
	struct packet_queue *udp_incoming, *udp_incoming_last;
	/* The size of the queue, and the offset into the current
	 * packet where the read pointer is. */
	int udp_incoming_size, udp_incoming_offset;

	struct intern_connection *free;
};

struct intern_connection *free_list;
struct intern_connection connections[MAX_CONNS];

static int net_conn_init(void)
{
	int i;

	free_list = &connections[0];
	for (i = 0; i < MAX_CONNS ; i++) {
		struct intern_connection *ic = &connections[i];
		ic->conn_type = FREE;
		ic->free = &connections[i+1];
	}
	connections[MAX_CONNS-1].free = NULL;

	return 0;
}

/* Return the opaque connection value exported to other components for
 * a given internal_connection */
static inline net_connection_t net_conn_get_opaque(struct intern_connection *ic)
{
	return ic - connections;
}

/* Get the internal representation of a connection */
static inline struct intern_connection *net_conn_get_internal(net_connection_t nc)
{
	assert(nc < MAX_CONNS && nc >= 0);

	return &connections[nc];
}

static inline int net_conn_valid(net_connection_t nc)
{
	if (nc < 0 || nc >= MAX_CONNS) return 0;
	return 1;
}

static inline struct intern_connection *net_conn_alloc(conn_t conn_type)
{
	struct intern_connection *ic = free_list;

	if (NULL == ic) return NULL;
	free_list = ic->free;
	ic->free = NULL;
	ic->udp_incoming = ic->udp_incoming_last = NULL;
	ic->udp_incoming_size = 0;
	ic->tid = cos_get_thd_id();
	ic->thd_status = ACTIVE;
	ic->conn_type = conn_type;

	return ic;
}

static inline void net_conn_free(struct intern_connection *ic)
{
	ic->conn_type = FREE;
	ic->free = free_list;
	free_list = ic;

	return;
}

/* 
 * The pbuf->payload might point to the actual data, but we might want
 * to free the data, which means we want to find the real start of the
 * data. Here we assume that p->payload does point to the start of the
 * data, not a header.  Also assume that no IP options are used. 
 */
static inline void *cos_net_header_start(struct pbuf *p, conn_t ct)
{
	char *data = (char *)p->payload;
	const int ip_sz = sizeof(struct ip_hdr),
		udp_sz = sizeof(struct udp_hdr),
		tcp_sz = sizeof(struct tcp_hdr);
	struct ip_hdr *possible_start;
	
	if (UDP == ct) {
		possible_start = (struct ip_hdr*)(data - ip_sz - udp_sz);
		if (IP_PROTO_UDP != IPH_PROTO(possible_start)) return NULL;
	} else {
		possible_start = (struct ip_hdr*)(data - ip_sz - tcp_sz);
		if (IP_PROTO_TCP != IPH_PROTO(possible_start)) return NULL;
	}
	/* sanity checks: */
	if (4 != IPH_V(possible_start)) return NULL;
	return possible_start;
}

static void cos_net_stack_udp_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
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
	if (ic->udp_incoming_size >= UDP_RCV_MAX) {
		free(headers);
		assert(p->ref > 0);
		pbuf_free(p);

		return;
	}
	/* We are going to use the space for the ip header to contain
	 * the packet queue structure */
	pq = (struct packet_queue *)headers;
	pq->data = p->payload;
	pq->len = p->len;
	pq->next = NULL;
	
//TEST	printc("pbuf %x (headers:%x) added to queue (from %x:%d)", p, p->payload, ip->addr, port);
	assert((NULL == ic->udp_incoming) == (NULL == ic->udp_incoming_last));
	/* Is the queue empty? */
	if (NULL == ic->udp_incoming) {
		assert(NULL == ic->udp_incoming_last);
		ic->udp_incoming = ic->udp_incoming_last = pq;
	} else {
		last = ic->udp_incoming_last;
		last->next = pq;
		ic->udp_incoming_last = pq;
	}
	ic->udp_incoming_size += p->len;
	assert(1 == p->ref);
	pbuf_free(p);

	/* If the thread blocked waiting for a packet, wake it up */
	if (BLOCKED == ic->thd_status) {
		ic->thd_status = ACTIVE;
		sched_wakeup(cos_spd_id(), ic->tid);
	}

	return;
//	pbuf_free(p);
//	printc("woohoo: packet with buffer @ %x, len %d, totlen %d (final: %x), first headers: %x!", 
//	       p->payload, p->len, p->tot_len, (char *)p->payload - (UDP_HLEN + IP_HLEN),
//	       *(unsigned int*)p->payload);
}

/* 
 * FIXME: currently we associate a connection with a thread (and an
 * upcall thread), which is restrictive.
 */
net_connection_t net_create_udp_connection(spdid_t spdid)
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
	ic = net_conn_alloc(UDP);
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
	ic->conn_type = UDP;
	ic->tm = tm;
	ic->conn.up = up;
	udp_recv(up, cos_net_stack_udp_recv, (void*)ic);
	return net_conn_get_opaque(ic);

conn_err:
	net_conn_free(ic);
udp_err:
	udp_remove(up);
err:
	return ret;
}

int net_bind(spdid_t spdid, net_connection_t nc, struct ip_addr *ip, u16_t port)
{
	struct udp_pcb *up;
	struct intern_connection *ic;
	u16_t tid = cos_get_thd_id();

	if (!net_conn_valid(nc)) return -EINVAL;
	ic = net_conn_get_internal(nc);
	if (tid != ic->tid) return -EPERM;

	if (UDP == ic->conn_type) {
		up = ic->conn.up;
		assert(up);
		if (ERR_OK != udp_bind(up, ip, port)) return -EPERM;
	} else {
		assert(0);
	}

	return 0;
}

int net_connect(spdid_t spdid, net_connection_t nc, struct ip_addr *ip, u16_t port)
{
	struct udp_pcb *up;
	struct intern_connection *ic;
	u16_t tid = cos_get_thd_id();
	
	if (!net_conn_valid(nc)) return -EINVAL;
	ic = net_conn_get_internal(nc);
	if (tid != ic->tid) return -EPERM;

	if (UDP == ic->conn_type) {
		up = ic->conn.up;
		if (ERR_OK != udp_connect(up, ip, port)) return -EISCONN;
	} else {
		assert(0);
	}

	return 0;
}

static int cos_net_udp_recv(struct intern_connection *ic, void *data, int sz)
{
	int xfer_amnt = 0;

	/* here we read the minimum of the size of the first pending
	 * packet, or the first (really, next) sz bytes of the first
	 * packet */
	while (1) {
		/* If there is data available, get it */
		if (ic->udp_incoming_size > 0) {
			struct packet_queue *p;
			char *data_start;
			int data_left;

			p = ic->udp_incoming;
			assert(p);
			//assert(p->ref > 0);
			/*INCORRECT KLUDGE*///	if (p->ref <= 0) pbuf_ref(p);
//			printc("consuming packet_queue %x", p);
			data_start = ((char*)p->data) + ic->udp_incoming_offset;
			data_left = p->len - ic->udp_incoming_offset;
			assert(data_left > 0 && data_left <= p->len);
			/* Consume all of first packet? */
			if (data_left <= sz) {
				ic->udp_incoming = p->next;
				if (ic->udp_incoming_last == p) {
					assert(NULL == ic->udp_incoming);
					ic->udp_incoming_last = NULL;
				}
				memcpy(data, data_start, data_left);
				xfer_amnt = data_left;
				ic->udp_incoming_offset = 0;

				free(p);
			} 
			/* Consume part of first packet */
			else {
				memcpy(data, data_start, sz);
				xfer_amnt = sz;
				ic->udp_incoming_offset += sz;
			}
			ic->udp_incoming_size -= xfer_amnt;

			break;
		}

		/* If there isn't data, block until woken by
		 * an interrupt in cos_net_stack_udp_recv */
		ic->thd_status = BLOCKED;
		lock_release(&net_lock);
		sched_block(cos_spd_id());
		lock_take(&net_lock);
	}

	return xfer_amnt;
}

int net_recv(spdid_t spdid, net_connection_t nc, void *data, int sz)
{
//	struct udp_pcb *up;
	struct intern_connection *ic;
	u16_t tid = cos_get_thd_id();
	int xfer_amnt = 0;

	if (!net_conn_valid(nc)) return -EINVAL;

	lock_take(&net_lock);
	ic = net_conn_get_internal(nc);
	if (tid != ic->tid) {
		lock_release(&net_lock);
		return -EPERM;
	}

	if (UDP == ic->conn_type) {
		//up = ic->conn.up;
		xfer_amnt = cos_net_udp_recv(ic, data, sz);
	} else {
		assert(0);
	}
	lock_release(&net_lock);

	return xfer_amnt;
}

int net_send(spdid_t spdid, net_connection_t nc, void *data, int sz)
{
	struct udp_pcb *up;
	struct intern_connection *ic;
	u16_t tid = cos_get_thd_id();
	int ret = sz;

	if (!net_conn_valid(nc)) return -EINVAL;
	if (sz > MAX_UDP_SEND) return -EMSGSIZE;

	lock_take(&net_lock);
	ic = net_conn_get_internal(nc);
	if (tid != ic->tid) {
		ret = -EPERM;
		goto err;
	}

	if (UDP == ic->conn_type) {
		struct pbuf *p;

		/* There's no blocking in the UDP case, so this is simple */
		up = ic->conn.up;
		p = pbuf_alloc(PBUF_TRANSPORT, sz, PBUF_REF);
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
	} else {
	  	assert(0);
	}
err:
	lock_release(&net_lock);
	return ret;
}

/************************ LWIP integration: **************************/

struct ip_addr ip, mask, gw;
struct netif   cos_if;

struct udp_pcb *upcb_200, *upcb_out;
struct tcp_pcb *tpcb_200, *tpcb_accept, *tpcb_out;

void cos_net_interrupt(void)
{
	unsigned short int ucid = cos_get_thd_id();
	void *buff, *d;
	int max_len, len;
	struct thd_map *tm;
	struct pbuf *p;//, *np;
	struct ip_hdr *ih;

	lock_take(&net_lock);

	tm = get_thd_map(ucid);
	assert(tm);
	if (rb_retrieve_buff(tm->uc_rb, &buff, &max_len)) {
		prints("net: could not retrieve buffer from ring.");
		lock_release(&net_lock);
		return;
	}

	ih = (struct ip_hdr*)buff;
	if (unlikely(4 != IPH_V(ih))) goto err;
	len = ntohs(IPH_LEN(ih));
	if (unlikely(len > MTU)) {
		printc("len %d > %d", len, MTU);
		goto err;
	}

	p = pbuf_alloc(PBUF_IP, len, PBUF_REF);
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
	d = malloc(len);
//TEST	printc("pushing pbuf %x w/ data %x of len %d", p, d, len);
	if (NULL != d) {
		memcpy(d, buff, len);
		p->payload = d;//buff;
		/* free packet in this call... */
		if (ERR_OK != cos_if.input(p, &cos_if)) {
			prints("net: failure in IP input.");
			lock_release(&net_lock);
			return;
		}
	} else {
		pbuf_free(p);
	}
//	printc("allocated packet data @ %x", d);
//	printc("Sending packet with payload @ %p, starting with %x", p->payload, *(unsigned int*)p->payload);

	/* Unlock, lock */

	/* The packet is processed, we are done with it */
/*	plen = 16;//p->len - (UDP_HLEN + IP_HLEN);
	np = pbuf_alloc(PBUF_TRANSPORT, plen, PBUF_REF);
	if (np) {
		np->payload = (char*)buff + (UDP_HLEN + IP_HLEN);
		if (ERR_OK != udp_send(upcb_out, np)) {
			prints("net: could not send data.");
		}
		pbuf_free(np);
	} else {
		assert(0);
	}
*/	/* OK, recycle the buffer. */
	if (rb_add_buff(tm->uc_rb, buff /*(char *)p->payload - (UDP_HLEN + IP_HLEN)*/, MTU)) {
		prints("net: could not add buffer to ring.");
	}
	lock_release(&net_lock);

	return;
err:
	/* Recycle the buffer (essentially dropping packet)... */
	if (rb_add_buff(tm->uc_rb, buff, MTU)) {
		prints("net: OOM, and filed to add buffer.");
	}
	lock_release(&net_lock);
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
	struct pbuf *data;

	/* We are requiring chained pbufs here, one for the header,
	 * one for the data.  First assert checks that we have > 1
	 * pbuf, second asserts we have 2 */

	/* Acks/etc might not work here as they might be one pbuf. */
	assert(p && p->next);
	data = p->next;
	assert(data->len == data->tot_len);
//	printc("header pkt len %d, data len %d", p->len, data->len);
	assert(p->len <= sizeof(xmit_headers.headers));
//	printc("sending w/ headers @ %p, and data @ %p(= to %x), len %d!", 
//	       p->payload, data->payload, *(unsigned int*)data->payload, data->len);

	/* assuming the net lock is taken here */
	memcpy(xmit_headers.headers, p->payload, p->len);
	xmit_headers.len = p->len;
	if (cos_buff_mgmt(data->payload, data->len, 0, COS_BM_XMIT)) {
		prints("net: could not xmit data.");
	}

	return ERR_OK;
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
		udp_recv(up, cos_net_stack_udp_recv, (void*)tm);
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
	udp_recv(up, cos_net_stack_udp_recv, (void*)tm);
	return up;
}

/* /\* TCP functions *\/ */

/* /\* Callback function for when we have actually received data *\/ */
/* static err_t cos_net_stack_tcp_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t e) */
/* { */
/* 	struct thd_map *tm; */
/* 	struct pbuf *np; */

/* 	if (ERR_OK != e) { */
/* 		print("net: tcp recv got error %d. %d%d", e, 0,0); */
/* 	} */

/* 	/\* Has the remote host closed the connection? *\/ */
/* 	if (NULL == p) { */
/* 		prints("net: tcp recv connection closed"); */
/* 	} else { */
/* 		pbuf_free(p); */
/* 	} */
/* 	/\* FIXME: This should be called by the application thread when */
/* 	 * it really reads the data. *\/ */
/* 	tcp_recved(tpcb, 0); */

/* 	prints("net: tcp received data!"); */
/* //	printc("woohoo: packet with buffer @ %x, len %d, totlen %d (final: %x), first data: %x!",  */
/* //	       p->payload, p->len, p->tot_len, (char *)p->payload - (UDP_HLEN + IP_HLEN), */
/* //	       *(unsigned int*)p->payload); */

/* 	return ERR_OK; */
/* } */

/* static err_t cos_net_stack_tcp_sent(void *arg, struct tcp_pcb *tp, u16_t len) */
/* { */
/* 	prints("net: successfully received by other end-point!"); */
/* 	return ERR_OK; */
/* } */

/* /\* Callback function when a connection is established *\/ */
/* static err_t cos_net_tcp_connected(void *arg, struct tcp_pcb *tpcb, err_t err) */
/* { */
/* 	prints("net: tcp connection connected"); */
/* 	return ERR_OK; */
/* } */

/* static struct tcp_pcb *cos_net_create_outbound_tcp_conn(u16_t local_port, u16_t remote_port,  */
/* 							struct ip_addr *ip, struct thd_map *tm) */
/* { */
/* 	struct tcp_pcb *tp; */

/* 	assert(tm && remote_port && ip); */
/* 	tp = tcp_new(); */
/* 	if (!tp) return NULL; */
/* 	if (local_port) { */
/* 		if (ERR_OK != tcp_bind(tp, IP_ADDR_ANY, local_port)) { */
/* 			prints("net: could not create outbound tcp connection (bind)."); */
/* 			if (ERR_OK != tcp_close(tp)) { */
/* 				prints("net: could not close pcb"); */
/* 			} */
/* 			return NULL; */
/* 		} */
/* 		tcp_recv(tp, cos_net_stack_tcp_recv); */
/* 		tcp_arg(tp, (void*)tm); */
/* 	} */
/* 	if (ERR_OK != tcp_connect(tp, ip, remote_port)) { */
/* 		prints("net: could not create outbound tcp connection (connect)."); */
/* 		if (ERR_OK != tcp_close(tp)) { */
/* 			prints("net: could not close pcb"); */
/* 		} */
/* 	} */
/* 	return tp; */
/* } */

/* /\* Callback function when we get an accept *\/ */
/* static err_t cos_net_tcp_accept(void *arg, struct tcp_pcb *new_tp, err_t err) */
/* { */
/* 	assert(arg && new_tp); */

/* 	tcp_accepted(new_tp); */
/* 	/\* arg should still be the struct thd_map *\/ */
/* 	tcp_recv(new_tp, cos_net_stack_tcp_recv); */
/* 	tcp_sent(new_tp, cos_net_stack_tcp_sent); */

/* 	return ERR_OK; */
/* } */

/* static struct tcp_pcb *cos_net_tcp_listen_with_backlog(struct tcp_pcb *tpcb, u8_t n) */
/* { */
/* 	struct tcp_pcb *new; */
	
/* 	assert(tpcb); */
/* 	new = tcp_listen_with_backlog(tpcb, n); */
/* 	assert(new); */
/* 	tcp_accept(tpcb, cos_net_tcp_accept); */

/* 	return new; */
/* } */

/* /\* Create pcb and bind it locally *\/ */
/* static struct tcp_pcb *cos_net_create_inbound_tcp_conn(u16_t local_port, struct thd_map *tm) */
/* { */
/* 	struct tcp_pcb *tp; */

/* 	assert(tm); */
/* 	tp = tcp_new(); */
/* 	if (!tp) return NULL; */
/* 	if (ERR_OK != tcp_bind(tp, IP_ADDR_ANY, local_port)) { */
/* 		prints("net: could not create inbound tcp_conn."); */
/* 		if (ERR_OK != tcp_close(tp)) { */
/* 			prints("net: could not close pcb"); */
/* 		} */
/* 		return NULL; */
/* 	} */
/* 	tcp_arg(tp, (void*)tm); */

/* 	return tp; */
/* } */

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

static void test_thd(void)
{
	net_connection_t nc, nco;
	char data[128], len;
	int ret;
	struct ip_addr dest;

	prints("Starting the test thread!");
	nc = net_create_udp_connection(cos_spd_id());
	if (nc) print("create udp connection error: %d %d%d", nc, 0,0);
	ret = net_bind(cos_spd_id(), nc, IP_ADDR_ANY, 200);
	if (ret) print("Bind error: %d. %d%d", ret, 0, 0);
	nco = net_create_udp_connection(cos_spd_id());
	IP4_ADDR(&dest, 10,0,1,5);
	net_connect(cos_spd_id(), nco, &dest, 6000);
	while (1) {
		len = net_recv(cos_spd_id(), nc, data, 128);
		net_send(cos_spd_id(), nco, data, len);
//		print("%d %d %d", len, len, len);
		//sched_block(cos_spd_id());
	}
}

static int init(void) 
{
	unsigned short int ucid, i;
	void *b;

	lock_static_init(&alloc_lock);
//	lock_static_init(&tmap_lock);
	lock_static_init(&net_lock);

	rb_init(&rb1_md_wildcard, &rb1);
	rb_init(&rb2_md, &rb2);
//	rb_init(&rb3_md, &rb3);

	net_conn_init();

	/* Setup the region from which headers will be transmitted. */
	if (cos_buff_mgmt(&xmit_headers, sizeof(xmit_headers), 0, COS_BM_XMIT_REGION)) {
		prints("net: error setting up xmit region.");
	}

	lock_take(&net_lock);
	/* Wildcard upcall */
	ucid = cos_net_create_net_upcall(0, &rb1_md_wildcard);
	if (ucid == 0) {
		lock_release(&net_lock);
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

	lock_release(&net_lock);
//	sched_block();
	while (1) {
		/* Sleep for a quarter of seconds as prescribed by lwip */
		timed_event_block(cos_spd_id(), 250000);
		lock_take(&net_lock);
		tcp_tmr();
		lock_release(&net_lock);
	}

	//sched_block();

	prints("net: Error -- returning from init!!!");
	assert(0);
	return 0;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
	{
		cos_net_interrupt();
		break;
	}
	case COS_UPCALL_BOOTSTRAP:
	{
		volatile static int first = 1;

		if (first) {
			first = 0;
			init();
			assert(0);
		} else {
			test_thd();
			prints("net: not expecting more than one bootstrap.");
		}
		break;
	}
	default:
		print("Unknown type of upcall %d made to net (%d, %d).", t, (unsigned int)arg1,(unsigned int)arg2);
		assert(0);
		return;
	}
	return;
}
