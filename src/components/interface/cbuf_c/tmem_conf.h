#ifndef TMEM_CONF_H
#define TMEM_CONF_H

#include <cbuf_c.h>

#define MAX_NUM_CBUFS 100
#define MAX_NUM_ITEMS MAX_NUM_CBUFS

#define TAKE() if(sched_component_take(cos_spd_id())) BUG();
#define RELEASE() if(sched_component_release(cos_spd_id())) BUG();

/* cos_lock_t l; */
/* #define TAKE() lock_take(&l); */
/* #define RELEASE() lock_release(&l); */

typedef struct cos_cbuf_item tmem_item;

/* Shared page between the target component, and us */
typedef	struct cbuf_vect_intern_struct shared_component_info;

/* /\* 1 means there's memory available in local cache *\/ */
/* #define MEM_IN_LOCAL_CACHE(sci) ((sci)->ci->cos_stacks.freelists[0].freelist != 0) */

typedef enum {
	CBUFM_LARGE = 1,
	CBUFM_RO    = 1<<1,
	CBUFM_GRANT = 1<<2,
	CBUFM_IN_USE = 1<<3,
	CBUFM_RELINQUISH = 1<<4
} cbufm_flags_t;

/* 
 * This data-structure is shared between this component and the cbuf_c
 * (the cbuf manager) and the refcnt is used to gauge if the cbuf is
 * actually in use.  The cbuf_c can garbage collect it if not (TODO).
 */
union cbuf_meta {
	u32_t v;        		/* value */
	struct {
		u32_t ptr:20, obj_sz:6; /* page pointer, and ... */
		/* the object size is the size of the object if it is
		 * <= the size of a page, OR the _order_ of the number
		 * of pages in the object, if it is > PAGE_SIZE */
	        cbufm_flags_t flags:6;
		/* int refcnt:1; */
	} __attribute__((packed)) c;	/* composite type */
};

struct cb_desc;
struct cb_mapping {
	spdid_t spd;
	vaddr_t addr;		/* other component's map address */
	struct cb_mapping *next, *prev;
	struct cb_desc *cbd;
};

/* Data we wish to track for every cbuf */
struct cb_desc {
	u16_t principal;	/* principal that owns the memory */
	int cbid;		/* cbuf id */
	int obj_sz;
	void *addr; 	/* local map address */
	struct cb_mapping owner;
};


struct cos_cbuf_item {
	struct cos_cbuf_item *next, *prev;
	struct cos_cbuf_item *free_next;
//	u32_t mapped;
//	u32_t flags;
//	vaddr_t d_addr;
	spdid_t parent_spdid;	
	struct cb_desc desc;
};

#endif
