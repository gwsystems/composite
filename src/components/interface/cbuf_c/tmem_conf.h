#ifndef TMEM_CONF_H
#define TMEM_CONF_H

#include <cbuf_c.h>
#include <cos_synchronization.h>

cos_lock_t tmem_l;
#define TAKE()  do { if (lock_take(&tmem_l) != 0) BUG(); } while(0)
#define RELEASE() do { if (lock_release(&tmem_l) != 0) BUG() } while(0)
#define LOCK_INIT()    lock_static_init(&tmem_l);

/* 
 * tmem_item in this case is a list of the cbufs that are _owned_ by a
 * specific spdid (as opposed to all that are mapped into it).
 */
typedef struct cos_cbuf_item tmem_item;

#define LOCAL_ADDR(cci) (cci->desc.addr)

/* Shared page between the target component, and us */
typedef	struct spd_cbvect_range shared_component_info;

#define TMEM_RELINQ COMP_INFO_TMEM_CBUF_RELINQ

#define CBUF_IN_USE(flags) (flags & CBUFM_IN_USE)

typedef enum {
	CBUFM_LARGE = 1,
	CBUFM_RO    = 1<<1,
	CBUFM_GRANT = 1<<2,
	CBUFM_IN_USE = 1<<3,
	CBUFM_RELINQUISH_TEST = 1<<4
} cbufm_flags_t;

/* 
 * This data-structure is shared between this component and the cbuf_c
 * (the cbuf manager) and the refcnt is used to gauge if the cbuf is
 * actually in use.  The cbuf_c can garbage collect it if not (TODO).
 */
/* union cbuf_meta { */
/* 	u32_t v;        		/\* value *\/ */
/* 	struct { */
/* 		u32_t ptr:20, obj_sz:6; /\* page pointer, and ... *\/ */
/* 		/\* the object size is the size of the object if it is */
/* 		 * <= the size of a page, OR the _order_ of the number */
/* 		 * of pages in the object, if it is > PAGE_SIZE *\/ */
/* 	        cbufm_flags_t flags:6; */
/* 		/\* int refcnt:1; *\/ */
/* 	} __attribute__((packed)) c;	/\* composite type *\/ */
/* }; */


union cbuf_meta {
	struct {
		u32_t v;        		/* value */
		u32_t th_id;
	} c_0;
	struct {
		u32_t ptr:20, obj_sz:6; /* page pointer, and ... */
		/* the object size is the size of the object if it is
		 * <= the size of a page, OR the _order_ of the number
		 * of pages in the object, if it is > PAGE_SIZE */
	        cbufm_flags_t flags:6;

		u32_t thdid_owner;
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
	void *addr; 	/* local map address, done at init*/
	struct cb_mapping owner;
};


struct cos_cbuf_item {
	struct cos_cbuf_item *next, *prev;
	struct cos_cbuf_item *free_next;
	union cbuf_meta *entry;    /* vector entry for quick lookup */
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
	union cbuf_meta *meta; /* sizeof == PAGE_SIZE, 512 entries */
	struct spd_cbvect_range *next, *prev;
};

#endif
