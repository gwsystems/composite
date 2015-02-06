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
#define CBUF_REFCNT_SZ     8
#define CBUF_FLAGS_SZ      4
#define CBUF_PTR_MASK      0xFFF
#define CBUF_INC_UNIT      (1<<CBUF_FLAGS_SZ)
#define CBUF_DEC_UNIT      (~(CBUF_INC_UNIT)+1)
#define CBUF_REFCNT_MAX    ((1<<CBUF_REFCNT_SZ)-1)
#define CBUF_SENDRECV_MAX  ((1<<8)-1)

/*get flags*/
#define CBUF_RELINQ(meta)            ((meta)->nfo & CBUF_RELINQ)
#define CBUF_OWNER(meta)             ((meta)->nfo & CBUF_OWNER)
#define CBUF_TMEM(meta)              ((meta)->nfo & CBUF_TMEM)
#define CBUF_INCONSISTENT(meta)       ((meta)->nfo & CBUF_INCONSISTENT)
/*set flags*/
#define CBUF_FLAG_ADD(meta, v)       ((meta)->nfo |= (v))
/*unset flags*/
#define CBUF_FLAG_REM(meta, v)       ((meta)->nfo &= (~(v)))

#define CBUF_REFCNT(meta)            (((meta)->nfo>>CBUF_FLAGS_SZ) & 0xFF)
#define CBUF_REFCNT_ATOMIC_INC(meta) cos_faa((int *)(&((meta)->nfo)), CBUF_INC_UNIT)
#define CBUF_REFCNT_ATOMIC_DEC(meta) cos_faa((int *)(&((meta)->nfo)), CBUF_DEC_UNIT)
#define CBUF_PTR(meta)               (((meta)->nfo)&(~CBUF_PTR_MASK))
#define CBUF_PTR_SET(meta, v)        ((meta)->nfo |= (v))
#define CBUF_NSND_INC(meta)          ((meta)->snd_rcv.nsent++)
#define CBUF_NRCV_INC(meta)          ((meta)->snd_rcv.nrecvd++)
#define CBUF_IS_MAPPED(meta)         (CBUF_PTR(meta) != 0)
#define CBUF_IS_IN_FREELiST(meta)    ((meta)->next != NULL)

/** 
 * manually assign each bit to prevent compiler reorder   
 *                                                        
 * structure of nfo in meta data as follows:              
 * +-------20-----+----8---+----------------4---------------+ 
 * | page pointer | refcnt | owner|tmem|inconsistent|relinq | 
 * +--------------+--------+--------------------------------+ 
 *
 * Shared page between the target component, and us */
typedef struct spd_cbvect_range shared_component_info;

typedef enum {
	/* 
	 * Should the current cbuf be freed back 
	 *to the manager when we're done?
	 */
	CBUF_RELINQ      = 1,
	/* 
	 * Is this cbuf inconsistent between 
	 * manager and client?
	 */
	CBUF_INCONSISTENT = 1<<1,
	/* 
	 * Is this a transient memory allocation? 
	 */
	CBUF_TMEM        = 1<<2,
	/* 
	 * Are we the originator for the mapping?
	 */
	CBUF_OWNER       = 1<<3
} cbuf_flags_t;
struct snd_rcv_info{
	u8_t nsent, nrecvd;
}__attribute__((packed));
union cbid_tag {
	u32_t cbid;
	unsigned int tag;
};
struct cbuf_meta {
	unsigned int nfo;
	u16_t sz;			/* # of pages */
	struct snd_rcv_info snd_rcv;
	struct cbuf_meta *next;    /*single circular linked list*/
	union cbid_tag cbid_tag;
}__attribute__((packed));

#endif /* CBUF_META_H */
