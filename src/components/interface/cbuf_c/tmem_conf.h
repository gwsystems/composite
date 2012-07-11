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
#define TMEM_TOUCHED(cci)  (cci->entry->nfo.c.flags & CBUFM_TOUCHED)
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
	CBUFM_RECVED   = 1<<4,
} cbufm_flags_t;

union cbufm_info {
	u32_t v;      /* value, for atomic manipulation */
	struct {
		u32_t         ptr  :20; /* page pointer */
		u32_t         nsent:7;  /* # times cbuf's been sent */
		cbufm_flags_t flags:5;
	} __attribute__((packed)) c;
};

struct cbuf_meta {
	union cbufm_info nfo;
	u16_t sz;			/* # of pages || 0 == TMEM */
	u16_t thdid_owner;
};

static inline 
int cbufm_is_mapped(struct cbuf_meta *m) { return m->nfo.c.ptr != 0; }
static inline
int cbufm_is_tmem(struct cbuf_meta *m)   { return m->sz == 0; }

struct cb_desc;
struct cb_mapping {
	spdid_t spd;
	vaddr_t addr;		/* other component's map address */
	struct cb_mapping *next, *prev;
	struct cb_desc *cbd;
};

/* Data we wish to track for every cbuf */
struct cb_desc {
	int cbid, sz;
	void *addr; 	/* local map address, done at init*/
	struct cb_mapping owner;
};


struct cos_cbuf_item {
	struct cos_cbuf_item *next, *prev;
	struct cos_cbuf_item *free_next;
	struct cbuf_meta *entry;    /* vector entry for quick lookup */
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
