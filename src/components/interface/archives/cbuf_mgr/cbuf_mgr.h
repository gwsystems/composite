/**
 * Copyright 2012 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2012
 */

#ifndef   	CBUF_MGR_H
#define   	CBUF_MGR_H

#include <cos_component.h>
#include <ck_ring_cos.h>

/* 
 * cbuf_create and cbuf_retrieve:
 * cbid is complicated --
 * 0: we don't know the cbuf id we want to create
 * n > 0: we want to initialize the cbuf_meta structure and get the cbuf addr
 *
 * The return value is negative (-c) if we need to create a new level
 * in the cbuf_meta vector for c.  This is expanded by calling
 * cbuf_register.  We expect that we will call this function again
 * with c to get the cbuf's address via a populated cbuf_meta.
 */
int cbuf_create(spdid_t spdid, unsigned long size, int cbid);
int cbuf_delete(spdid_t spdid, unsigned int cbid);
int cbuf_retrieve(spdid_t spdid, unsigned int cbid, unsigned long len);
vaddr_t cbuf_register(spdid_t spdid, unsigned int cbid);

/* Map a cbuf into another component at a given address.
 * The s_spd that calls this function should ensure the memory is not freed.
 * The d_addr must be alloced with sufficient pages to contain the cbuf.
 */
vaddr_t cbuf_map_at(spdid_t s_spd, unsigned int cbid, spdid_t d_spd, vaddr_t d_addr);
int cbuf_unmap_at(spdid_t s_spd, unsigned int cbid, spdid_t d_spd, vaddr_t d_addr);

vaddr_t cbuf_fork_spd(spdid_t spd, spdid_t s_spd, spdid_t d_spd, int cinfo_cbid);

/*
 * Before the first call to cbuf_collect, the client component must
 * call cbuf_map_collect in order to map the shared page used to
 * return the list of garbage-collected cbufs.
 */
vaddr_t cbuf_map_collect(spdid_t spdid);

/* 
 * When we have no more cbufs of a specific size, lets try and
 * collect the ones we've given away. This function returns a positive value
 * for the number of cbufs collected, 0 if non are available, or a
 * negative value for an error.
 */
int cbuf_collect(spdid_t spdid, unsigned long size);

/*set limit size of cbuf in component spdid to target_size*/
void cbuf_mempool_resize(spdid_t spdid, unsigned long target_size);
unsigned long cbuf_memory_target_get(spdid_t spdid);

/* Collected cbufs are stored in a page shared between cbufp and clients.
 * A ring buffer data structure is put in the first part of the page.
 * The rest of the page contains the buffer of collected cbuf_t identifiers,
 * but with integer pointer types for easy conversion to ring buffer types.
 * The buffer must be a power of 2, but since the ring structure is stored
 * in the page, there is only PAGE_SIZE - (sizeof(struct ck_ring)) space
 * available. Thus the buffer is allocated to be half a page, and there
 * remains some available space if needed.
 */
struct cbuf_ring_element {
	intptr_t cbid;
};
CK_RING(cbuf_ring_element, cbuf_ring);

struct cbuf_shared_page {
	CK_RING_INSTANCE(cbuf_ring) ring;
#define CSP_BUFFER_SIZE ((PAGE_SIZE>>1)/sizeof(struct cbuf_ring_element))
	struct cbuf_ring_element buffer[CSP_BUFFER_SIZE];
};

#define OP_NUM 10
typedef enum {
	CBUF_CRT = 0,
	CBUF_COLLECT,
	CBUF_DEL,
	CBUF_RETRV,
	CBUF_REG,
	CBUF_MAP
} cbuf_debug_t;

typedef enum {
	CBUF_TARGET = 0,
	CBUF_ALLOC,
	CBUF_USE,
	CBUF_GARBAGE,
	CBUF_BLK,
	CBUF_RELINQ_NUM,
	BLK_THD_NUM,
	TOT_BLK_TSC,
	MAX_BLK_TSC,
	TOT_GC_TSC,
	MAX_GC_TSC
} cbuf_policy_t;

#endif 	    /* !CBUF_MGR_H */
