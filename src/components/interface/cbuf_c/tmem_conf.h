/**
 * Copyright 2011 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2011
 */

#ifndef TMEM_CONF_H
#define TMEM_CONF_H

#include <cbuf_c.h>
#include <cos_synchronization.h>

extern cos_lock_t tmem_l;
#define LOCK_INIT() lock_static_init(&tmem_l);
#define TAKE()      do { if (lock_take(&tmem_l))    BUG(); } while(0)
#define RELEASE()   do { if (lock_release(&tmem_l)) BUG(); } while(0)

/* 
 * tmem_item in this case is a list of the cbufs that are _owned_ by a
 * specific spdid (as opposed to all that are mapped into it).
 */
typedef struct cos_cbuf_item tmem_item;

#include <cbuf_meta.h>

struct cb_desc;
struct cb_mapping {
	spdid_t spd;
	vaddr_t addr;		/* other component's map address */
	struct cbuf_meta *meta; /* vector entry for quick lookup */
	struct cb_mapping *next, *prev;
	struct cb_desc *cbd;
};

enum {
	CBUF_DESC_TMEM = 0x1
};

/* Data we wish to track for every cbuf */
struct cb_desc {
	int flags;
	int cbid, sz;
	void *addr; 	/* local map address, done at init*/
	struct cb_mapping owner;
};

struct cos_cbuf_item {
	struct cos_cbuf_item *next, *prev, *free_next;
	spdid_t parent_spdid;	
	struct cb_desc desc;
};

/* 
 * A linked list of the cbuf_vect second level pages -- the
 * data-structure used to track cbufs, that is mapped between the
 * client and the server -- that tracks for a given cbuf_id, which
 * page represents that mapping.
 */
struct spd_cbvect_range {
	long start_id, end_id;
	struct cos_component_information *spd_cinfo_page;
	struct cbuf_meta *meta; /* sizeof == PAGE_SIZE, 512 entries */
	struct spd_cbvect_range *next, *prev;
};

#endif
