/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_FMT_PRINT 

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#include <cos_list.h>
#include <cos_synchronization.h>

#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/udp.h>

#include <string.h>

#define NUM_THDS MAX_NUM_THREADS
#define NUM_WILDCARD_BUFFS 64 //32

/* 
 * We need page-aligned data for the network ring buffers.  This
 * structure lies at the beginning of a page and describes the data in
 * it.  When amnt_buffs = 0, we can dealloc the page.
 */
#define NP_NUM_BUFFS 2
#define MTU 1500
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
static rb_meta_t rb1_md, rb2_md, rb3_md;
static ring_buff_t rb1, rb2, rb3;

cos_lock_t tmap_lock;
struct thd_map {
	unsigned short int thd, upcall, port;
	rb_meta_t *uc_rb;
} tmap[NUM_THDS];

cos_lock_t alloc_lock;


struct cos_net_xmit_headers xmit_headers;

/******************* Manipulations for the thread map: ********************/

static struct thd_map *get_thd_map_port(unsigned short port) 
{
	int i;

	lock_take(&tmap_lock);
	for (i = 0 ; i < NUM_THDS ; i++) {
		if (tmap[i].port == port) {
			lock_release(&tmap_lock);
			return &tmap[i];
		}
	}
	lock_release(&tmap_lock);
	
	return NULL;
}

static struct thd_map *get_thd_map(unsigned short int thd_id)
{
	int i;
	
	lock_take(&tmap_lock);
	for (i = 0 ; i < NUM_THDS ; i++) {
		if (tmap[i].thd    == thd_id ||
		    tmap[i].upcall == thd_id) {
			lock_release(&tmap_lock);
			return &tmap[i];
		}
	}
	lock_release(&tmap_lock);
	
	return NULL;
}

static int add_thd_map(unsigned short int ucid, unsigned short int port, rb_meta_t *rbm)
{
	int i;
	
	lock_take(&tmap_lock);
	for (i = 0 ; i < NUM_THDS ; i++) {
		if (tmap[i].thd == 0) {
			tmap[i].thd    = cos_get_thd_id();
			tmap[i].upcall = ucid;
			tmap[i].port   = port;
			tmap[i].uc_rb  = rbm;
			break;
		}
	}
	lock_release(&tmap_lock);
	assert(i != NUM_THDS);

	return 0;
}

static int rem_thd_map(unsigned short int tid)
{
	struct thd_map *tm;

	/* Utilizing recursive locks here... */
	lock_take(&tmap_lock);
	tm = get_thd_map(tid);
	if (!tm) {
		lock_release(&tmap_lock);
		return -1;
	}
	tm->thd = 0;
	lock_release(&tmap_lock);

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
static int rb_retrieve_buff(rb_meta_t *r, void **buf, int *len)
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
	*len = rbb->len;
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
extern int sched_block(void);
extern int sched_wakeup(unsigned short int thd_id);

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


/************************ LWIP integration: **************************/

struct ip_addr ip, mask, gw;
struct netif   cos_if;
cos_lock_t     net_lock;

struct udp_pcb *upcb_200, *upcb_out;

void cos_net_interrupt(void)
{
	unsigned short int ucid = cos_get_thd_id();
	void *buff, *pbuff;
	int len, plen;
	struct thd_map *tm;
	struct pbuf *p, *np;

	tm = get_thd_map(ucid);
	assert(tm);
	if (rb_retrieve_buff(tm->uc_rb, &buff, &len)) {
		prints("net: could not retrieve buffer from ring.");
		return;
	}

	p = pbuf_alloc(PBUF_IP, len, PBUF_REF);
	if (!p) {
		prints("OOM in interrupt: allocation of pbuf failed.\n");
		/* Recycle the buffer (essentially dropping packet)... */
		if (rb_add_buff(tm->uc_rb, buff, MTU)) {
			prints("net: OOM, and filed to add buffer.");
		}
		return;
	}
	p->payload = buff;
	/* free packet in this call... */
	if (ERR_OK != cos_if.input(p, &cos_if)) {
		prints("net: failure in IP input.");
		return;
	}
//	printc("Sending packet with payload @ %p, starting with %x", p->payload, *(unsigned int*)p->payload);

	/* Unlock, lock */

	/* The packet is processed, we are done with it */
	plen = 16;//p->len - (UDP_HLEN + IP_HLEN);
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
	/* OK, recycle the buffer. */
	if (rb_add_buff(tm->uc_rb, buff /*(char *)p->payload - (UDP_HLEN + IP_HLEN)*/, MTU)) {
		prints("net: could not add buffer to ring.");
	}

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
//	struct thd_map *tm;
	struct pbuf *data;

	/* We are requiring chained pbufs here, one for the header,
	 * one for the data.  First assert checks that we have > 1
	 * pbuf, second asserts we have 2 */
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

static void cos_net_stack_udp_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
				   struct ip_addr *ip, u16_t port)
{
	struct thd_map *tm;
	struct pbuf *np;

	pbuf_free(p);
//	printc("woohoo: packet with buffer @ %x, len %d, totlen %d (final: %x), first data: %x!", 
//	       p->payload, p->len, p->tot_len, (char *)p->payload - (UDP_HLEN + IP_HLEN),
//	       *(unsigned int*)p->payload);
}

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
		}
		udp_recv(up, cos_net_stack_udp_recv, (void*)tm);
	}
	if (ERR_OK != udp_connect(up, ip, remote_port)) {
		prints("net: could not create outbound udp connection (connect).");
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
		return NULL;
	}
	udp_recv(up, cos_net_stack_udp_recv, (void*)tm);
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
	struct ip_addr dest;

	lwip_init();

	IP4_ADDR(&ip, 10,0,2,8);
	IP4_ADDR(&gw, 10,0,1,1); //korma
	IP4_ADDR(&mask, 255,255,255,0);
	
	netif_add(&cos_if, &ip, &mask, &gw, NULL, cos_if_init, ip_input);
	netif_set_default(&cos_if);
	netif_set_up(&cos_if);

	upcb_200 = cos_net_create_inbound_udp_conn(200, get_thd_map(cos_get_thd_id()));
	IP4_ADDR(&dest, 10,0,1,6);
	upcb_out = cos_net_create_outbound_udp_conn(0, 6000, &dest, get_thd_map(cos_get_thd_id()));
}

static int init(void) 
{
	unsigned short int ucid, i;
	void *b;

	lock_static_init(&alloc_lock);
	lock_static_init(&tmap_lock);
	lock_static_init(&net_lock);

	rb_init(&rb1_md, &rb1);
	rb_init(&rb2_md, &rb2);
	rb_init(&rb3_md, &rb3);

	/* Setup the region from which headers will be transmitted. */
	if (cos_buff_mgmt(&xmit_headers, sizeof(xmit_headers), 0, COS_BM_XMIT_REGION)) {
		prints("net: error setting up xmit region.");
	}

	/* Wildcard upcall */
	ucid = cos_net_create_net_upcall(0, &rb1_md);
	if (ucid == 0) return 0;
	add_thd_map(ucid, 0 /* wildcard port */, &rb1_md);
	
	init_lwip();

	for (i = 0 ; i < NUM_WILDCARD_BUFFS ; i++) {
		if(!(b = alloc_rb_buff(&rb1_md))) {
			prints("net: could not allocate the ring buffer.");
		}
		if(rb_add_buff(&rb1_md, b, MTU)) {
			prints("net: could not populate the ring with buffer");
		}
	}

	sched_block();

	prints("net: returning from init!!!");
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
		static int first = 1;

		if (first) {
			init();
			assert(0);
			first = 0;
		} else {
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
