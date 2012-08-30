#ifndef TMEM_CONF_H
#define TMEM_CONF_H

#include <cbuf_c.h>
#include <cos_synchronization.h>

extern cos_lock_t tmem_l;
#define LOCK_INIT() lock_static_init(&tmem_l);
#define TAKE()      do { if (lock_take(&tmem_l))    BUG(); } while(0)
#define RELEASE()   do { if (lock_release(&tmem_l)) BUG() } while(0)

/* 
 * tmem_item in this case is a list of the cbufs that are _owned_ by a
 * specific spdid (as opposed to all that are mapped into it).
 */
typedef struct cos_cbuf_item tmem_item;

#define CBUF_OWNER(flags)  (flags & CBUFM_OWNER)
#define CBUF_IN_USE(flags) (flags & CBUFM_IN_USE)
#define LOCAL_ADDR(cci)    (cci->desc.addr)
#define TMEM_TOUCHED(cci)  (cci->desc.owner.meta->nfo.c.flags & CBUFM_TOUCHED)
#define TMEM_RELINQ        COMP_INFO_TMEM_CBUF

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
	CBUFM_IN_USE   = 1<<2,
	/* 
	 * Has the cbuf been used?
	 * Invariant: ptr != 0
	 */
	CBUFM_TOUCHED  = 1<<3,
	/* 
	 * Is this a transient memory allocation? 
	 * Invariant: ptr != 0, sz = 0
	 */
	CBUFM_TMEM     = 1<<4,
	/* 
	 * Should the current cbuf be freed back to the manager when
	 * we're done?  Normally, using tmem, we use the relinquish
	 * bit in the component information page, but sometimes
	 * (non-tmem cases) we need to be more specific and do this on
	 * a buffer-to-buffer basis.
	 * Invariant: !TMEM && sz != 0
	 */
	CBUFM_RELINQ   = 1<<5,
	CBUFM_MAX      = 1<<6
} cbufm_flags_t;

union cbufm_info {
	u32_t v;      /* value, for atomic manipulation */
	struct {
		u32_t         ptr  :20; /* page pointer */
		cbufm_flags_t flags:12;
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
