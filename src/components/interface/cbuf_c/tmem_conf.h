#ifndef TMEM_CONF_H
#define TMEM_CONF_H

#include <cbuf_c.h>
//#include <lock.h>

#define MAX_NUM_CBUFS 100

typedef struct cos_cbuf_item tmem_item;

/* Shared page between the target component, and us */
typedef	struct cbuf_vect_intern_struct shared_component_info;


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
	vaddr_t d_addr;   //  address in other spds that created?
	spdid_t parent_spdid;	
	struct cb_desc *desc_ptr;
};

/* cos_lock_t l; */
/* #define TAKE() lock_take(&l); */
/* #define RELEASE() lock_release(&l); */

#endif
