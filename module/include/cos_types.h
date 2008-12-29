/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef TYPES_H
#define TYPES_H

#include "debug.h"
#include "measurement.h"

#ifndef COS_BASE_TYPES
#define COS_BASE_TYPES
typedef unsigned char      u8_t;
typedef unsigned short int u16_t;
typedef unsigned int       u32_t;
typedef unsigned long long u64_t;
#endif

struct shared_user_data {
	unsigned int current_thread;
	void *argument_region;
	unsigned int brand_principal;
	unsigned int current_cpu;
};

struct cos_sched_next_thd {
	volatile unsigned short int next_thd_id, next_thd_flags;
	volatile unsigned int next_thd_urgency;
};

#define COS_SCHED_EVT_NEXT(evt)    (evt)->nfu.v.next
#define COS_SCHED_EVT_FLAGS(evt)   (evt)->nfu.v.flags
#define COS_SCHED_EVT_URGENCY(evt) (evt)->nfu.v.urgency
#define COS_SCHED_EVT_VALS(evt)    (evt)->nfu.c.vals

/* If the urgency is set to this, the upcall is disabled */
#define COS_SCHED_EVT_DISABLED_VAL (0xFFFF)

/* FIXME: make flags 8 bits, and use 8 bits to count # of alive upcalls */
#define COS_SCHED_EVT_FREE         0x1
#define COS_SCHED_EVT_EXCL         0x2
#define COS_SCHED_EVT_BRAND_ACTIVE 0x4
#define COS_SCHED_EVT_BRAND_READY  0x8
#define COS_SCHED_EVT_BRAND_PEND   0x10
#define COS_SCHED_EVT_NIL          0x20

/* Must all fit into a word */
struct cos_se_values {
	volatile u8_t next, flags;
	volatile u16_t urgency;
} __attribute__((packed));

struct cos_sched_events {
	union next_flags_urg {
		volatile struct cos_se_values v;
		struct compressed {
			u32_t vals;
		} c;
	} nfu;
	u32_t cpu_consumption;
} __attribute__((packed));

/* Primitive for scheduler synchronization.  These must reside in the same word */
struct cos_synchronization_atom {
	volatile unsigned short int owner_thd, queued_thd;
} __attribute__((packed));

#define NUM_SCHED_EVTS 128 //256

struct cos_sched_data_area {
	struct cos_sched_next_thd cos_next; //[NUM_CPUS];
	struct cos_synchronization_atom cos_locks; //[NUM_CPUS];
	struct cos_sched_events cos_events[NUM_SCHED_EVTS]; // maximum of PAGE_SIZE/sizeof(struct cos_sched_events) - ceil(sizeof(struct cos_sched_curr_thd)/(sizeof(struct cos_sched_events)+sizeof(locks)))
} __attribute__((packed,aligned(4096)));

#ifndef NULL
#define NULL ((void*)0)
#endif

/* 
 * Ring buffer is structured as such (R is RB_READY, U is RB_USED, E is RB_EMPTY):
 * +-> EEEEUUUUUURRRRRRREEEE-+
 * |                         |
 * +-------------------------+
 * where empty cells contain no useful information, used signal
 * buffers that have packet data placed into them, and ready cells are
 * ready to receive data.  It is the responsibility of the component
 * to maintain this.  The kernel will simply linearly walk along
 * looking for ready cells and mark them as used when it places data
 * in their buffers.
 *
 * This is a hack to interface with the Linux packet handling.
 */
enum {
	RB_EMPTY = 0,
	RB_READY,
	RB_USED,
	RB_ERR
};
#define RB_SIZE (4096 / 8) /* 4096 / sizeof(struct rb_buff_t), or 512 */
/* HACK: network ring buffer */
/* 
 * TODO: Needed in this structure: a way to just turn off the stream,
 * a binary switch that the user-level networking stack can use to
 * tell the lower-layers to just drop following packets, until the bit
 * is unset.  (this should be simple as the mechanism already exists
 * for when there are no open slots in the rb, to drop and not make an
 * upcall, so we just need to hook into that.)
 */
typedef struct {
	struct rb_buff_t {
		void *ptr;
		unsigned short int len, status;
	} __attribute__((packed)) packets[RB_SIZE];
} __attribute__((aligned(4096))) ring_buff_t ;



/*
 * For interoperability with the networking side.  This is the brand
 * port/brand thread pair, and the callback structures for
 * communcation.
 */
struct cos_brand_info {
	unsigned short int  brand_port;
	struct thread      *brand;
	void               *private;
};
typedef void (*cos_net_data_completion_t)(void *data);
struct cos_net_callbacks {
	int (*xmit_packet)(void *headers, int hlen, void *user_buffer, int len);
	int (*create_brand)(struct cos_brand_info *bi);
	int (*remove_brand)(struct cos_brand_info *bi);

	/* depricated: */
	int (*get_packet)(struct cos_brand_info *bi, char **packet, unsigned long *len,
			  cos_net_data_completion_t *fn, void **data, unsigned short int *port);
};

/*
 * These types are for addresses that are never meant to be
 * dereferenced.  They will generally be used to set up page table
 * entries.
 */
typedef unsigned long phys_addr_t;
typedef unsigned long vaddr_t;
typedef unsigned int page_index_t;

typedef unsigned short int spdid_t;
typedef unsigned short int thdid_t;

typedef enum {
	COS_UPCALL_BRAND_EXEC,
	COS_UPCALL_BRAND_COMPLETE,
	COS_UPCALL_BOOTSTRAP,
	COS_UPCALL_CREATE,
	COS_UPCALL_DESTROY
} upcall_type_t;

/* operations for cos_brand_cntl and cos_brand_upcall */
enum {
/* cos_brand_cntl -> */
	COS_BRAND_CREATE,
	COS_BRAND_ADD_THD,
	COS_BRAND_CREATE_HW,
/* cos_brand_upcall -> */
	COS_BRAND_TAILCALL,  /* tailcall brand to upstream spd
			      * (don't maintain this flow of control).
			      * Not sure if this would work with non-brand threads
			      */
	COS_BRAND_ASYNC,     /* async brand while maintaining control */
	COS_BRAND_UPCALL     /* continue executing an already made
			      * brand, redundant with tail call? */
};

enum {
	COS_HW_TIMER,
	COS_HW_NET,
	COS_UC_NOTIF
};

struct cos_net_xmit_headers {
	/* Length of the header */
	int len;
	/* Max IP header len + max TCP header len */
	char headers[80];
}__attribute__((aligned(4096)));

enum {
	COS_BM_XMIT,
	COS_BM_XMIT_REGION,
	COS_BM_RECV_RING
};

/* operations for cos_sched_cntl */
enum {
	COS_SCHED_EVT_REGION,
	COS_SCHED_THD_EVT,
	COS_SCHED_GRANT_SCHED,
	COS_SCHED_REVOKE_SCHED
};

/* flags for cos_switch_thread */
#define COS_SCHED_EXCL_YIELD   0x1
#define COS_SCHED_TAILCALL     0x2
#define COS_SCHED_SYNC_BLOCK   0x4
#define COS_SCHED_SYNC_UNBLOCK 0x8

struct mpd_split_ret {
	short int new, old;
} __attribute__((packed));

static inline int mpd_split_error(struct mpd_split_ret ret)
{
	return (ret.new < 0) ? 1 : 0;
}

/* operations for manipulating mpds */
enum {
//	COS_MPD_START_TRANSACTION,
//	COS_MPD_END_TRANSACTION,
	COS_MPD_SPLIT,
	COS_MPD_MERGE,
	COS_MPD_SPLIT_MERGE,
	COS_MPD_DEMO,
	COS_MPD_DEBUG
//	COS_MPD_ISOLATE
};

enum {
	COS_MMAP_GRANT,
	COS_MMAP_REVOKE
};

#define IL_INV_UNMAP (0x1) // when invoking, should we be unmapped?
#define IL_RET_UNMAP (0x2) // when returning, should we unmap?
#define MAX_ISOLATION_LVL_VAL (IL_INV_UNMAP|IL_RET_UNMAP)

/*
 * Note on Symmetric Trust, Symmetric Distruct, and Asym trust:
 * ST  iff (flags & (CAP_INV_UNMAP|CAP_RET_UNMAP) == 0)
 * SDT iff (flags & CAP_INV_UNMAP && flags & CAP_RET_UNMAP)
 * AST iff (!(flags & CAP_INV_UNMAP) && flags & CAP_RET_UNMAP)
 */
#define IL_ST  (0)
#define IL_SDT (IL_INV_UNMAP|IL_RET_UNMAP)
#define IL_AST (IL_RET_UNMAP)
/* invalid type, can NOT be used in data structures, only for return values. */
#define IL_INV (~0)
typedef unsigned int isolation_level_t;

#define CAP_SAVE_REGS 0x1

#ifdef __KERNEL__
#include <asm/atomic.h>
#else

typedef struct { volatile unsigned int counter; } atomic_t;

#endif /* __KERNEL__ */

static inline void cos_ref_take(atomic_t *rc)
{
	rc->counter++;
	cos_meas_event(COS_MPD_REFCNT_INC);
}

static inline void cos_ref_set(atomic_t *rc, unsigned int val)
{
	rc->counter = val;
}

static inline unsigned int cos_ref_val(atomic_t *rc)
{
	return rc->counter;
}

static inline void cos_ref_release(atomic_t *rc)
{
	rc->counter--; /* assert(rc->counter != 0) */
	cos_meas_event(COS_MPD_REFCNT_DEC);
}

#endif /* TYPES_H */
