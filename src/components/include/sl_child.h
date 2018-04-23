/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#ifndef SL_CHILD_H
#define SL_CHILD_H

#include <ck_ring.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>

#define SL_CHILD_RBUF_SZ   (64 * sizeof(struct sl_child_notification))
#define SL_CHILD_RING_SZ   (sizeof(struct ck_ring) + SL_CHILD_RBUF_SZ)
#define SL_CHILD_SHM_PAGES (round_up_to_page(SL_CHILD_RING_SZ)/PAGE_SIZE)
#define SL_CHILD_SHM_SZ    (PAGE_SIZE * SL_CHILD_SHM_PAGES)

typedef enum {
	SL_CHILD_THD_BLOCK = 0,
	SL_CHILD_THD_WAKEUP,
} sl_child_notif_t;

struct sl_child_notification {
	sl_child_notif_t type;
	thdid_t          tid;
};

CK_RING_PROTOTYPE(child, sl_child_notification);

/* in-place ring-buff! NO POINTER PASSING!! */
static inline struct ck_ring *
sl_child_ring(vaddr_t vaddr)
{
	return (struct ck_ring *)(vaddr);
}

static inline struct sl_child_notification *
sl_child_notif_buffer(vaddr_t vaddr)
{
	return (struct sl_child_notification *)(vaddr + sizeof(struct ck_ring));
}

/* parent creates a shared memory region, initializes for ring buffers and returns cbuf id */
cbuf_t sl_parent_notif_alloc(struct sl_thd *childthd);
/* API for a parent to produce */
int    sl_parent_notif_enqueue(struct sl_thd *thd, struct sl_child_notification *notif);
/* child maps the shared memory region for consuming */
int    sl_child_notif_map(cbuf_t id);
/* API for a child consumer */
int    sl_child_notif_dequeue(struct sl_child_notification *notif);
/* is the ring empty or no requests in the ring */
int    sl_child_notif_empty(void);

/* produce notification and asnd to the child */
int sl_parent_notif_block_no_cs(struct sl_thd *child, struct sl_thd *thd);
int sl_parent_notif_wakeup_no_cs(struct sl_thd *child, struct sl_thd *thd);

#endif /* SL_CHILD_H */
