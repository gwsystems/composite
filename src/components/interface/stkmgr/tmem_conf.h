#ifndef TMEM_CONF_H
#define TMEM_CONF_H

#include <stkmgr.h>

typedef struct cos_stk_item tmem_item;

/* Shared page between the target component, and us */
typedef	struct cos_component_information shared_component_info;

/* 1 means there's memory available in local cache */
#define MEM_IN_LOCAL_CACHE(ssi) ((ssi)->ci->cos_stacks.freelists[0].freelist != 0)

/**
 * This struct maps directly to how the memory
 * is layed out and used in memory
 */
struct cos_stk {
	struct cos_stk *next;
	u32_t flags;
	u32_t thdid_owner;
} __attribute__((packed));

#define D_COS_STK_ADDR(d_addr) (d_addr + PAGE_SIZE - sizeof(struct cos_stk))

/**
 * Information aobut a stack
 */
struct cos_stk_item {
	struct cos_stk_item *next, *prev; /* per-spd list */
	struct cos_stk_item *free_next; /* freelist of tmem manager */
	spdid_t parent_spdid;       // Not needed but saves on lookup
	vaddr_t d_addr;
	void *hptr;
	struct cos_stk *stk;
};

#endif
