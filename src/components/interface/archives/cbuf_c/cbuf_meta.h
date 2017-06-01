/**
 * Copyright 2012 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2012
 */

#ifndef CBUF_META_H
#define CBUF_META_H

#define CBUF_OWNER(flags)  (flags & CBUFM_OWNER)
#define LOCAL_ADDR(cci)    (cci->desc.addr)
#define TMEM_TOUCHED(cci)  (cci->desc.owner.meta->nfo.c.flags & CBUFM_TOUCHED)
#define TMEM_RELINQ        COMP_INFO_TMEM_CBUF
#define CBUFM_REFCNT_SZ     7
#define CBUFM_FLAGS_SZ      5
#define CBUFP_REFCNT_MAX   ((1<<CBUFM_REFCNT_SZ)-1)

/* Shared page between the target component, and us */
typedef	struct spd_cbvect_range shared_component_info;

typedef enum {
	/* 
	 * Are we the originator for the mapping?
	 * Invariant: !CBUFM_RO
	 */
	CBUFM_OWNER    = 1,
	/* 
	 * Is the mapping writable, or read-only?
	 * Invariant: CBUFM_OWNER by default
	 */
	CBUFM_WRITABLE = 1<<1,
	/* 
	 * This is marked when the component is currently accessing
	 * the cbuf and assumes that it is mapped in.  This is in some
	 * ways a lock preventing the cbuf manager from removing the
	 * cbuf.
	 * Invariant: ptr != 0
	 */
	/* 
	 * Has the cbuf been used?
	 * Invariant: ptr != 0
	 */
	CBUFM_TOUCHED  = 1<<2,
	/* 
	 * Is this a transient memory allocation? 
	 * Invariant: ptr != 0, sz = 0
	 */
	CBUFM_TMEM     = 1<<3,
	/* 
	 * Should the current cbuf be freed back to the manager when
	 * we're done?  Normally, using tmem, we use the relinquish
	 * bit in the component information page, but sometimes
	 * (non-tmem cases) we need to be more specific and do this on
	 * a buffer-to-buffer basis.
	 * Invariant: !TMEM && sz != 0
	 */
	CBUFM_RELINQ   = 1<<4
} cbufm_flags_t;

union cbufm_info {
	u32_t v;      /* value, for atomic manipulation */
	struct {
		u32_t         ptr  :20; /* page pointer */
		u32_t         refcnt:CBUFM_REFCNT_SZ;
		cbufm_flags_t flags: CBUFM_FLAGS_SZ;
	} __attribute__((packed)) c;
};

#define TMEM_SENDRECV_MAX  ((1<<8)-1)

union cbufm_ownership {
	u16_t thdid;        	/*  CBUFM_TMEM && sz == 0 */
	struct {
		u8_t nsent, nrecvd;
	} c; 		        /* !CBUFM_TMEM && sz > 0 */
};

struct cbuf_meta {
	union cbufm_info nfo;
	u16_t sz;			/* # of pages || 0 == TMEM */
	union cbufm_ownership owner_nfo;
};

static inline 
int cbufm_is_mapped(struct cbuf_meta *m) { return m->nfo.c.ptr != 0; }
static inline
int cbufm_is_tmem(struct cbuf_meta *m)   { return m->sz == 0; }

#endif /* CBUF_META_H */
