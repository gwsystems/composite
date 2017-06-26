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

#define CBUF_MAX_NSZ       64 	/* maximum number of sizes cbuf support. */
#define CBUF_REFCNT_SZ     7
#define CBUF_FLAGS_SZ      5
#define CBUF_PTR_MASK      0xFFF
#define CBUF_REFCNT_MAX    ((1<<CBUF_REFCNT_SZ)-1)
#define CBUF_SENDRECV_MAX  ((1<<8)-1)

/*get flags*/
#define CBUF_RELINQ(meta)             ((meta)->nfo & CBUF_RELINQ)
#define CBUF_OWNER(meta)              ((meta)->nfo & CBUF_OWNER)
#define CBUF_TMEM(meta)               ((meta)->nfo & CBUF_TMEM)
#define CBUF_INCONSISTENT(meta)        ((meta)->nfo & CBUF_INCONSISTENT)
#define CBUF_EXACTSZ(meta)            ((meta)->nfo & CBUF_EXACTSZ)
/*set flags*/
#define CBUF_FLAG_ADD(meta, v)        ((meta)->nfo |= (v))
#define CBUF_FLAG_ATOMIC_ADD(meta, v) cos_atomic_or((int *)(&(meta)->nfo), (v))
/*unset flags*/
#define CBUF_FLAG_REM(meta, v)        ((meta)->nfo &= (~(v)))
#define CBUF_FLAG_ATOMIC_REM(meta, v) cos_atomic_and((int *)(&(meta)->nfo), ~(v))

#define CBUF_REFCNT(meta)             ((meta)->nfo & CBUF_REFCNT_MAX)
#define CBUF_REFCNT_INC(meta)         ((meta)->nfo += 1)
#define CBUF_REFCNT_DEC(meta)         ((meta)->nfo -= 1)
#define CBUF_REFCNT_ATOMIC_INC(meta)  cos_faa((int *)(&((meta)->nfo)), 1)
#define CBUF_REFCNT_ATOMIC_DEC(meta)  cos_faa((int *)(&((meta)->nfo)), -1)
#define CBUF_PTR(meta)                (((meta)->nfo) & (~CBUF_PTR_MASK))
#define CBUF_NSND_ATOMIC_INC(meta)    cos_faa_byte(&((meta)->snd_rcv.nsent), 1)
#define CBUF_NRCV_ATOMIC_INC(meta)    cos_faa_byte(&((meta)->snd_rcv.nrecvd), 1)
#define CBUF_IS_MAPPED(meta)          (CBUF_PTR(meta) != 0)
#define CBUF_IS_IN_FREELIST(meta)     ((meta)->next != NULL)

#define CBUF_PTR_SET(meta, v)         do {   \
	(meta)->nfo &= CBUF_PTR_MASK;        \
	(meta)->nfo |= (v); } while(0)

/** 
 * manually assign each bit to prevent compiler reorder   
 *                                                        
 * structure of nfo in meta data as follows:              
 *
 * +-------20-----+------------------5---------------------+---7----+ 
 * | page pointer | exactsz|owner|tmem|inconsistent|relinq | refcnt |
 * +--------------+----------------------------------------+--------+ 
 *
 * Shared page between the target component, and us */
typedef struct spd_cbvect_range shared_component_info;

typedef enum {
	/* 
	 * Should the current cbuf be freed back 
	 *to the manager when we're done?
	 */
	CBUF_RELINQ      = 1<< CBUF_REFCNT_SZ,
	/* 
	 * Is this cbuf inconsistent between 
	 * manager and client?
	 */
	CBUF_INCONSISTENT = 1<< (CBUF_REFCNT_SZ+1),
	/* 
	 * Is this a transient memory allocation? 
	 */
	CBUF_TMEM        = 1<< (CBUF_REFCNT_SZ+2),
	/* 
	 * Are we the originator for the mapping?
	 */
	CBUF_OWNER       = 1<< (CBUF_REFCNT_SZ+3),
	/* 
	 * Is this is an exact size cbuf
	 */
	CBUF_EXACTSZ     = 1<< (CBUF_REFCNT_SZ+4)

} cbuf_flags_t;
struct snd_rcv_info{
	u8_t nsent, nrecvd;
}__attribute__((packed));

union cbid_tag {
	unsigned int cbid;
	unsigned int tag;
};

struct cbuf_meta {
	unsigned long nfo;
	u16_t sz;			/* # of pages */
	struct snd_rcv_info snd_rcv;
	struct cbuf_meta *next;    /*single circular linked list*/
	union cbid_tag cbid_tag;
}__attribute__((packed));

#endif /* CBUF_META_H */
