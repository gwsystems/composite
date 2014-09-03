/**
 * Copyright 2009 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2009
 */

//#define UPCALL_TIMING 1

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

#include <string.h>
#include <errno.h>

#include <net_if.h>

#define NUM_WILDCARD_BUFFS 256 //64 //32
#define UDP_RCV_MAX (1<<15)
/* 
 * We need page-aligned data for the network ring buffers.  This
 * structure lies at the beginning of a page and describes the data in
 * it.  When amnt_buffs = 0, we can dealloc the page.
 */
#define NP_NUM_BUFFS 2
#define MTU 1500
#define MAX_SEND MTU
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
static rb_meta_t rb1_md_wildcard, rb2_md;
static ring_buff_t rb1, rb2;
static int wildcard_acap_id;

//cos_lock_t tmap_lock;
struct thd_map {
	rb_meta_t *uc_rb;
};

COS_VECT_CREATE_STATIC(tmap);

cos_lock_t netif_lock;

#define NET_LOCK_TAKE()    \
	do {								\
		if (lock_take(&netif_lock)) prints("error taking net lock."); \
	} while(0)

#define NET_LOCK_RELEASE() \
	do {								\
		if (lock_release(&netif_lock)) prints("error releasing net lock."); \
	} while (0)

struct cos_net_xmit_headers xmit_headers;

/******************* Manipulations for the thread map: ********************/
/* This structure allows an upcall thread to find its associated ring
 * buffers
 */
static struct thd_map *get_thd_map(unsigned short int thd_id)
{
	return cos_vect_lookup(&tmap, thd_id);
}

static int add_thd_map(unsigned short int ucid, rb_meta_t *rbm)
{
	struct thd_map *tm;

	tm = malloc(sizeof(struct thd_map));
	if (NULL == tm) return -1;

	tm->uc_rb = rbm;
	if (0 > cos_vect_add_id(&tmap, tm, ucid)) {
		free(tm);
		return -1;
	}

	return 0;
}

static int rem_thd_map(unsigned short int tid)
{
	struct thd_map *tm;

	tm = cos_vect_lookup(&tmap, tid);
	if (NULL == tm) return -1;
	free(tm);
	if (cos_vect_del(&tmap, tid)) return -1;

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
static int rb_retrieve_buff(rb_meta_t *r, unsigned int **buf, int *max_len)
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

	page = alloc_page();
	if (!page) {
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
	BUG();
}

#include <sched.h>

static int cos_net_create_net_acap(unsigned short int port, rb_meta_t *rbm)
{
	int acap;

	acap = cos_async_cap_cntl(COS_ACAP_CREATE, cos_spd_id(), cos_spd_id(), cos_get_thd_id());
	assert(acap);
	/* cli acap not used. The server acap will be triggered by
	 * network driver. */
	wildcard_acap_id = acap & 0xFFFF;
	assert(wildcard_acap_id > 0);

	if (sched_create_net_acap(cos_spd_id(), wildcard_acap_id, port)) return -1;
	if (cos_buff_mgmt(COS_BM_RECV_RING, rb1.packets, sizeof(rb1.packets), wildcard_acap_id)) {
		prints("net: could not setup recv ring.\n");
		return -1;
	}
	return 0;
}

static int __netif_xmit(char *d, unsigned int sz)
{
	/* If we're just transmitting a TCP packet without data
	 * (e.g. ack), then use the fast path here */
	assert(d && sz > 0);
	xmit_headers.len = 0;
	if (sz <= sizeof(xmit_headers.headers)) {
		memcpy(&xmit_headers.headers, d, sz);
		xmit_headers.len = sz;
		xmit_headers.gather_len = 0;
	} else {
		struct gather_item *gi;
		gi = &xmit_headers.gather_list[0];
		gi->data = d;
		gi->len = sz;
		xmit_headers.gather_len = 1;
	}
	
	/* 
	 * Here we do 2 things: create a separate gather data entry
	 * for each packet, and separate the data in individual
	 * packets into separate gather entries if it crosses page
	 * boundaries.  
	 *
	 * This is a general implementation for gather of packets.  We
	 * are, of course, currently doing something simpler.
	 */
/* 	for (i = 0 ; p && i < XMIT_HEADERS_GATHER_LEN ; i++) { */
/* 		char *data = p->payload; */
/* 		struct gather_item *gi = &xmit_headers.gather_list[i]; */
/* 		int len_on_page; */

/* 		assert(data && p->len < PAGE_SIZE); */
/* 		gi->data = data; */
/* 		gi->len  = p->len; */
/* 		len_on_page = (unsigned long)round_up_to_page(data) - (unsigned long)data; */
/* 		/\* Data split across pages??? *\/ */
/* 		if (len_on_page < p->len) { */
/* 			int len_on_second = p->len - len_on_page; */

/* 			if (XMIT_HEADERS_GATHER_LEN == i+1) goto segment_err; */
/* 			gi->len  = len_on_page; */
/* 			gi = gi+1; */
/* 			gi->data = data + len_on_page; */
/* 			gi->len  = len_on_second; */
/* 			i++; */
/* 		} */
/* 		assert(p->type != PBUF_POOL); */
/* 		assert(p->ref == 1); */
/* 		p = p->next; */
/* 	} */
/* 	if (unlikely(NULL != p)) goto segment_err; */
/* 	xmit_headers.gather_len = i; */


	/* Send the collection of pbuf data on its way. */
	if (cos_buff_mgmt(COS_BM_XMIT, NULL, 0, 0)) {
		prints("net: could not xmit data.\n");
	}

	return 0;
/* segment_err: */
/* 	printc("net: attempted to xmit too many segments"); */
/* 	goto done; */
}

static int interrupt_process(void *d, int sz, int *recv_len)
{
	unsigned short int ucid = cos_get_thd_id();
	unsigned int *buff;
	int max_len;
	struct thd_map *tm;
	unsigned int len;

	assert(d);

	tm = get_thd_map(ucid);
	assert(tm);
	if (rb_retrieve_buff(tm->uc_rb, &buff, &max_len)) {
		prints("net: could not retrieve buffer from ring.\n");
		goto err;
	}
	len = buff[0];
	*recv_len = len;
	if (unlikely(len > MTU)) {
		printc("len %d > %d\n", len, MTU);
		goto err_replace_buff;
	}
	memcpy(d, &buff[1], len);

	/* OK, recycle the buffer. */
	if (rb_add_buff(tm->uc_rb, buff, MTU)) {
		prints("net: could not add buffer to ring.");
	}

	return 0;

err_replace_buff:
	/* Recycle the buffer (essentially dropping packet)... */
	if (rb_add_buff(tm->uc_rb, buff, MTU)) {
		prints("net: OOM, and filed to add buffer.");
	}
err:
	return -1;
}

#ifdef UPCALL_TIMING
u32_t last_upcall_cyc;
#endif

unsigned long netif_upcall_cyc(void)
{
#ifdef UPCALL_TIMING
	u32_t t = last_upcall_cyc;
	last_upcall_cyc = 0;
	return t;
#else
	return 0;
#endif
}

static int interrupt_wait(void)
{
	int ret;

	assert(wildcard_acap_id > 0);
	if (-1 == (ret = cos_areceive(wildcard_acap_id))) BUG();
#ifdef UPCALL_TIMING
	last_upcall_cyc = (u32_t)ret;
#endif	
	return 0;
}

/* 
 * Currently, this only adds to the wildcard acap.
 */
int netif_event_create(spdid_t spdid)
{
	unsigned short int ucid = cos_get_thd_id();

	NET_LOCK_TAKE();

	/* Wildcard upcall */
	if (cos_net_create_net_acap(0, &rb1_md_wildcard)) BUG();
	assert(wildcard_acap_id > 0);
	add_thd_map(ucid, /*0 wildcard port ,*/ &rb1_md_wildcard);
	NET_LOCK_RELEASE();
	printc("created net uc %d associated with acap %d\n", ucid, wildcard_acap_id);

	return 0;
}

int netif_event_release(spdid_t spdid)
{
	assert(wildcard_acap_id > 0);
	
	NET_LOCK_TAKE();
	rem_thd_map(cos_get_thd_id());
	NET_LOCK_RELEASE();

	return 0;
}

int netif_event_wait(spdid_t spdid, struct cos_array *d)
{
	int ret_sz = 0;

	if (!cos_argreg_arr_intern(d)) return -EINVAL;
	if (d->sz < MTU) return -EINVAL;

	interrupt_wait();
	NET_LOCK_TAKE();
	if (interrupt_process(d->mem, d->sz, &ret_sz)) BUG();
	NET_LOCK_RELEASE();
	d->sz = ret_sz;

	return 0;
}

int netif_event_xmit(spdid_t spdid, struct cos_array *d)
{
	int ret;

	if (!cos_argreg_arr_intern(d)) return -EINVAL;
	if (d->sz > MTU || d->sz <= 0) return -EINVAL;

	NET_LOCK_TAKE();
	ret = __netif_xmit(d->mem, (unsigned int)d->sz);
	NET_LOCK_RELEASE();

	return ret;
}

/*** Initialization routines: ***/

static int init(void) 
{
	unsigned short int i;
	void *b;

	lock_static_init(&netif_lock);

	NET_LOCK_TAKE();

	cos_vect_init_static(&tmap);
	
	rb_init(&rb1_md_wildcard, &rb1);
	rb_init(&rb2_md, &rb2);

	/* Setup the region from which headers will be transmitted. */
	if (cos_buff_mgmt(COS_BM_XMIT_REGION, &xmit_headers, sizeof(xmit_headers), 0)) {
		prints("net: error setting up xmit region.");
	}

	for (i = 0 ; i < NUM_WILDCARD_BUFFS ; i++) {
		if(!(b = alloc_rb_buff(&rb1_md_wildcard))) {
			prints("net: could not allocate the ring buffer.");
		}
		if(rb_add_buff(&rb1_md_wildcard, b, MTU)) {
			prints("net: could not populate the ring with buffer");
		}
	}

	NET_LOCK_RELEASE();

	return 0;
}

void cos_init(void *arg)
{
	static volatile int first = 1;
	
	if (first) {
		first = 0;
		init();
	} else {
		prints("net: not expecting more than one bootstrap.");
	}
}

