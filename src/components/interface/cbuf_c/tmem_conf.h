#ifndef TMEM_CONF_H
#define TMEM_CONF_H

#include <cbuf_c.h>

#define MAX_NUM_CBUFS 100

typedef struct cos_cbuf_item tmem_item;

/* Shared page between the target component, and us */
typedef	struct cbuf_vect_t shared_component_info;

/* /\* 1 means there's memory available in local cache *\/ */
/* #define MEM_IN_LOCAL_CACHE(sci) ((sci)->ci->cos_stacks.freelists[0].freelist != 0) */

typedef enum {
	CBUFM_LARGE = 1,
	CBUFM_RO    = 1<<1,
	CBUFM_GRANT = 1<<2,
	CBUFM_IN_USE = 1<<3,
	CBUFM_RELINQUISH = 1<<4
} cbufm_flags_t;


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
	u32_t mapped;
	u32_t flags;
	vaddr_t d_addr;
	spdid_t parent_spdid;	
	struct cb_desc *desc_ptr;
};

/* cos_lock_t l; */
/* #define TAKE() lock_take(&l); */
/* #define RELEASE() lock_release(&l); */

#endif
