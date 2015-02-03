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
#define CBUFM_REFCNT_SZ     9
#define CBUFM_FIRST_FLAGS_SZ      3
#define CBUFM_SECOND_FLAGS_SZ      4
#define CBUFM_NEXT_MASK 0xf
#define CBUFM_PTR_MASK 0xfff
#define CBUFM_INC_UNIT     (1<<CBUFM_FIRST_FLAGS_SZ)
#define CBUFM_DEC_UNIT     (~(CBUFM_INC_UNIT)+1)
#define CBUF_REFCNT_MAX   ((1<<CBUFM_REFCNT_SZ)-1)
#define CBUF_SENDRECV_MAX  ((1<<8)-1)

/*get flags*/
#define CBUF_OWNER(meta)  ((meta)->nfo & CBUFM_OWNER)
#define CBUF_TMEM(meta) ((meta)->nfo & CBUFM_TMEM)
#define CBUF_INCONSISENT(meta) ((meta)->nfo & CBUFM_INCONSISENT)
#define CBUF_WRITABLE(meta) ((int)((meta)->next_flag) & CBUFM_WRITABLE)
#define CBUF_TOUCHED(meta) ((int)((meta)->next_flag) & CBUFM_TOUCHED)
#define CBUF_RELINQ(meta) ((int)((meta)->next_flag) & CBUFM_RELINQ)
/*set flags*/
#define CBUF_SET_OWNER(meta) (meta)->nfo |= CBUFM_OWNER
#define CBUF_SET_TMEM(meta) (meta)->nfo |= CBUFM_TMEM
#define CBUF_SET_INCONSISENT(meta) (meta)->nfo |= CBUFM_INCONSISENT
#define CBUF_SET_WRITABLE(meta) (meta)->next_flag = (struct cbuf_meta *)((int)(meta)->next_flag | CBUFM_WRITABLE)
#define CBUF_SET_TOUCHED(meta) (meta)->next_flag = (struct cbuf_meta *)((int)(meta)->next_flag | CBUFM_TOUCHED)
#define CBUF_SET_RELINQ(meta) (meta)->next_flag = (struct cbuf_meta *)((int)(meta)->next_flag | CBUFM_RELINQ)
/*unset flags*/
#define CBUF_UNSET_OWNER(meta) (meta)->nfo &= (~CBUFM_OWNER)
#define CBUF_UNSET_TMEM(meta) (meta)->nfo &= (~CBUFM_TMEM)
#define CBUF_UNSET_INCONSISENT(meta) (meta)->nfo = (int)(meta)->nfo & (~CBUFM_INCONSISENT)
#define CBUF_UNSET_WRITABLE(meta) (meta)->next_flag = (struct cbuf_meta *)((int)(meta)->next_flag & (~CBUFM_WRITABLE))
#define CBUF_UNSET_TOUCHED(meta) (meta)->next_flag = (struct cbuf_meta *)((int)(meta)->next_flag & (~CBUFM_TOUCHED))
#define CBUF_UNSET_RELINQ(meta) (meta)->next_flag = (struct cbuf_meta *)((int)(meta)->next_flag & (~CBUFM_RELINQ))


#define CBUFM_GET_PTR(meta) (((meta)->nfo)>>12)
#define CBUFM_SET_PTR(meta, v) do {         \
	(meta)->nfo &= CBUFM_PTR_MASK;      \
	(meta)->nfo |= (v); } while(0)        
#define CBUFM_GET_REFCNT(meta) (((meta)->nfo>>CBUFM_FIRST_FLAGS_SZ) & 0x1ff)
#define CBUFM_INC_REFCNT(meta) cos_faa((int *)(&((meta)->nfo)), CBUFM_INC_UNIT)
#define CBUFM_DEC_REFCNT(meta) cos_faa((int *)(&((meta)->nfo)), CBUFM_DEC_UNIT)
#define CBUFM_GET_NEXT(meta) ((struct cbuf_meta *)((int)((meta)->next_flag) & ~CBUFM_NEXT_MASK))
#define CBUFM_SET_NEXT(meta, next) do {                \
	(meta)->next_flag = (struct cbuf_meta *)((int)((meta)->next_flag) & CBUFM_NEXT_MASK);   \
	(meta)->next_flag = (struct cbuf_meta *)((int)((meta)->next_flag) | (int)(next)); } while(0)
#define CBUFM_SET_NEXT_NULL(meta) (meta)->next_flag = (struct cbuf_meta *)((int)((meta)->next_flag) & CBUFM_NEXT_MASK)

/** 
 * manually assign each bit to prevent compiler reorder   
 *                                                        
 * structure of nfo in meta data as follows:              
 * +-------20-----+-----9-----+---------3---------------+ 
 * | page pointer |  refcnt   | owner|tmem|inconsistent | 
 * +--------------+-----------+-------------------------+ 
 *
 * structure of next_flag in meta data as follows:              
 * +-------28------+-----------------4------------------+ 
 * | next pointer  |  writable| touched| relinq| unused | 
 * +---------------+------------------------------------+ 
 * Shared page between the target component, and us */
typedef	struct spd_cbvect_range shared_component_info;

typedef enum {
	/* 
	 * Is this cbuf inconsistent between 
	 * manager and client?
	 */
	CBUFM_INCONSISENT = 1,
	/* 
	 * Is this a transient memory allocation? 
	 * Invariant: ptr != 0, sz = 0
	 */
	CBUFM_TMEM        = 1<<1,
	/* 
	 * Are we the originator for the mapping?
	 * Invariant: !CBUFM_RO
	 */
	CBUFM_OWNER       = 1<<2
} cbufm_first_flags_t;

typedef enum {
	CBUFM_UNUSED  = 1,
	/* 
	 * Should the current cbuf be freed back to the manager when
	 * we're done?  Normally, using tmem, we use the relinquish
	 * bit in the component information page, but sometimes
	 * (non-tmem cases) we need to be more specific and do this on
	 * a buffer-to-buffer basis.
	 * Invariant: !TMEM && sz != 0
	 */
	CBUFM_RELINQ   = 1<<1,
	/* 
	 * This is marked when the component is currently accessing
	 * the cbuf and assumes that it is mapped in.  This is in some
	 * ways a lock preventing the cbuf manager from removing the
	 * cbuf.
	 * Invariant: ptr != 0
	 */
	CBUFM_TOUCHED  = 1<<2,
	/* 
	 * Is the mapping writable, or read-only?
	 * Invariant: CBUFM_OWNER by default
	 */
	CBUFM_WRITABLE = 1<<3
} cbufm_second_flags_t;

union cbufm_ownership {
	u16_t thdid;        	/*  CBUFM_TMEM && sz == 0 */
	struct {
		u8_t nsent, nrecvd;
	} c; 		        /* !CBUFM_TMEM && sz > 0 */
};
union cbid_tag {
	u32_t cbid;
	unsigned int tag;
};
struct cbuf_meta {
	unsigned int nfo;
	u16_t sz;			/* # of pages || 0 == TMEM */
	union cbufm_ownership owner_nfo;
	struct cbuf_meta *next_flag;    /*single circular linked list*/
	union cbid_tag cbid;
}__attribute__((packed));

static inline 
int cbufm_is_mapped(struct cbuf_meta *m) { return (m->nfo)>>12 != 0; }
static inline
int cbufm_is_tmem(struct cbuf_meta *m)   { return m->sz == 0 && (((m->nfo) & CBUFM_TMEM) != 0); }
static inline 
int cbufm_is_in_freelist(struct cbuf_meta *m) { return CBUFM_GET_NEXT(m) != 0; }

#endif /* CBUF_META_H */
