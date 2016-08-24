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

typedef u32_t cbuf_t; /* should match cbuf_t, and fit in a register */

/* 
 * These are more or less identical to the counterparts in cbuf_c.h,
 * so have a look at the documentation there.
 */
int cbuf_create(spdid_t spdid, int size, long cbid);
int cbuf_delete(spdid_t spdid, int cbid);
int cbuf_retrieve(spdid_t spdid, int cbid, int size);
vaddr_t __cbuf_register(spdid_t spdid, long cbid);
vaddr_t cbuf_register(spdid_t spdid, long cbid);

/* 
 * Map a cbufp into another component at a given address.
 * The s_spd that calls this function should ensure the memory is not freed.
 * The d_addr must be alloced with sufficient pages to contain the cbuf.
 */
vaddr_t cbuf_map_at(spdid_t s_spd, cbuf_t cbid, spdid_t d_spd, vaddr_t d_addr);
int cbuf_unmap_at(spdid_t s_spd, cbuf_t cbid, spdid_t d_spd, vaddr_t d_addr);

int cbuf_fork_spd(spdid_t spd, spdid_t s_spd, spdid_t d_spd);

/*
 * Before the first call to cbufp_collect, the client component must
 * call cbufp_map_collect in order to map the shared page used to
 * return the list of garbage-collected cbufs.
 */
vaddr_t cbuf_map_collect(spdid_t spdid);

/* 
 * When we have no more cbufps of a specific size, lets try and
 * collect the ones we've given away. This function returns a positive value
 * for the number of cbufs collected, 0 if non are available, or a
 * negative value for an error.
 */
int cbuf_collect(spdid_t spdid, int size);

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

/* GAP #include <cbuf_vect.h> */
/* #include <mem_mgr_large.h> */
/* /\* Included mainly for struct cbuf_meta: *\/ */
/* #include "../cbuf_c/tmem_conf.h" */

#endif 	    /* !CBUF_MGR_H */
