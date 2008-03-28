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

struct shared_user_data {
	unsigned int current_thread;
	void *argument_region;
	unsigned int current_cpu;
};

struct cos_sched_next_thd {
	unsigned short int next_thd_id, next_thd_flags;
	unsigned int next_thd_urgency;
};

struct cos_sched_events {
	struct cos_sched_events *next_thd;
	unsigned int cpu_consumption;
};

struct cos_synchronization_atom {
	volatile unsigned short int owner_thd, queued_thd;
} __attribute__((packed));

struct cos_sched_data_area {
	struct cos_sched_next_thd cos_next; //[NUM_CPUS];
	struct cos_synchronization_atom locks; //[NUM_CPUS];
//	struct cos_sched_events cos_events[256]; // maximum of PAGE_SIZE/sizeof(struct cos_sched_events) - ceil(sizeof(struct cos_sched_curr_thd)/(sizeof(struct cos_sched_events)+sizeof(locks)))
};

#ifndef NULL
#define NULL ((void*)0)
#endif

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
	COS_UPCALL_BOOTSTRAP
} upcall_type_t;

/* operations for cos_brand_cntl and cos_brand_upcall */
enum { 
	/* cos_brand_cntl -> */
	COS_BRAND_CREATE, 
	COS_BRAND_ADD_THD,

	/* cos_brand_upcall -> */
	COS_BRAND_TAILCALL, /* tailcall brand to upstream spd
			     * (don't maintain this flow of control) */
	COS_BRAND_ASYNC,    /* async brand while maintaining control */
	COS_BRAND_UPCALL    /* continue executing an already made brand */
};

/* operations for cos_sched_cntl */
enum { 
	COS_SCHED_EVT_REGION, 
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
