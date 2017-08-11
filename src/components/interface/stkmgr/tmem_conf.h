#ifndef TMEM_CONF_H
#define TMEM_CONF_H

#include <stkmgr.h>
#include <cos_synchronization.h>

cos_lock_t tmem_l;
#define TAKE()                                      \
	do {                                        \
		if (lock_take(&tmem_l) != 0) BUG(); \
	} while (0)
#define RELEASE()                                     \
	do {                                          \
		if (lock_release(&tmem_l) != 0) BUG() \
	} while (0)
#define LOCK_INIT() lock_static_init(&tmem_l);

typedef struct cos_stk_item tmem_item;

/* Shared page between the target component, and us */
typedef struct ci_wrapper_ptr shared_component_info;

/* typedef	struct cos_component_information shared_component_info; */

#define LOCAL_ADDR(csi) (csi->hptr)
#define TMEM_TOUCHED(csi) (csi->stk->flags & TOUCHED)
#define TMEM_RELINQ COMP_INFO_TMEM_STK

/**
 * Flags to control stack
 */
enum stk_flags
{
	IN_USE    = (0x01 << 0),
	TOUCHED   = (0x01 << 1), /* don't change the sequence here! Using in stk stub */
	PERMANATE = (0x01 << 2),
	MONITOR   = (0x01 << 3),
};

/**
 * This struct maps directly to how the memory
 * is layed out and used in memory
 */
struct cos_stk {
	struct cos_stk *next;
	u32_t           flags;
	u32_t           thdid_owner;
	u32_t           cpu_id;
} __attribute__((packed));

#define D_COS_STK_ADDR(d_addr) (d_addr + PAGE_SIZE - sizeof(struct cos_stk))

/**
 * Information aobut a stack
 */
struct cos_stk_item {
	struct cos_stk_item *next, *prev;  /* per-spd list */
	struct cos_stk_item *free_next;    /* freelist of tmem manager */
	spdid_t              parent_spdid; // Not needed but saves on lookup
	vaddr_t              d_addr;
	void *               hptr;
	struct cos_stk *     stk;
};


struct ci_wrapper_ptr {
	struct cos_component_information *spd_cinfo_page;
};

#endif
