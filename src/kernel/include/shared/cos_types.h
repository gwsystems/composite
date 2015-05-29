/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

/* 
 * Note that this file is included both by the kernel and by
 * components.  Unfortunately, that means that ifdefs might need to be
 * used to maintain the correct defines.
 */

#ifndef TYPES_H
#define TYPES_H

#include "./consts.h"

#include "../debug.h"

#ifndef COS_BASE_TYPES
#define COS_BASE_TYPES
typedef unsigned char      u8_t;
typedef unsigned short int u16_t;
typedef unsigned int       u32_t;
typedef unsigned long long u64_t;
typedef signed char      s8_t;
typedef signed short int s16_t;
typedef signed int       s32_t;
typedef signed long long s64_t;
#endif

typedef int cpuid_t; /* Don't use unsigned type. We use negative values for error cases. */

/* Macro used to define per core variables */
#define PERCPU(type, name)                              \
	PERCPU_DECL(type, name);                        \
	PERCPU_VAR(name)

#define PERCPU_DECL(type, name)                         \
struct __##name##_percore_decl {                        \
	type name;                                      \
} CACHE_ALIGNED

#define PERCPU_VAR(name)                                \
struct __##name##_percore_decl name[NUM_CPU]

/* With attribute */
#define PERCPU_ATTR(attr, type, name)	   	        \
	PERCPU_DECL(type, name);                        \
	PERCPU_VAR_ATTR(attr, name)

#define PERCPU_VAR_ATTR(attr, name)                     \
attr struct __##name##_percore_decl name[NUM_CPU]

/* when define an external per cpu variable */
#define PERCPU_EXTERN(name)		                \
	PERCPU_VAR_ATTR(extern, name)

/* We have different functions for getting current CPU in user level
 * and kernel. Thus the GET_CURR_CPU is used here. It's defined
 * separately in user(cos_component.h) and kernel(per_cpu.h).*/
#define PERCPU_GET(name)                (&(name[GET_CURR_CPU].name))
#define PERCPU_GET_TARGET(name, target) (&(name[target].name))

#include "../measurement.h"

struct shared_user_data {
	unsigned int current_thread;
	void *argument_region;
	unsigned int current_cpu;
};

struct cos_sched_next_thd {
	volatile u16_t next_thd_id, next_thd_flags;
};

#define COS_SCHED_EVT_NEXT(evt)    (evt)->nfu.v.next
#define COS_SCHED_EVT_FLAGS(evt)   (evt)->nfu.v.flags
#define COS_SCHED_EVT_URGENCY(evt) (evt)->nfu.v.urgency
#define COS_SCHED_EVT_VALS(evt)    (evt)->nfu.c.vals

/* FIXME: make flags 8 bits, and use 8 bits to count # of alive upcalls */
#define COS_SCHED_EVT_FREE         0x1
#define COS_SCHED_EVT_EXCL         0x2
#define COS_SCHED_EVT_ACAP_ACTIVE 0x4
#define COS_SCHED_EVT_ACAP_READY  0x8
#define COS_SCHED_EVT_ACAP_PEND   0x10
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

/* Primitive for scheduler synchronization.  These must reside in the
 * same word.  queued_thd is only accessed implicitly in the RAS
 * sections, so a text search for it won't give much information. */
union cos_synchronization_atom {
	struct {
		volatile u16_t owner_thd, queued_thd;
	} c;
	volatile u32_t v;
} __attribute__((packed));

/* 
 * If the pending_event value is set, then another scheduling event
 * has occurred.  These can include events such as asynchronous
 * invocations, or parent events (child thread blocks, wakes up,
 * etc...  When events are parsed, or the parent is polled for events,
 * this value should be cleared.  When a scheduling decision is made
 * and switch_thread is invoked, if this is set, then the switch will
 * not happen, and an appropriate return value will be returned.  If
 * the pending_cevt flag is set, then the parent has triggered an
 * event since we last checked for them.
 */
struct cos_event_notification {
	volatile u32_t pending_event, pending_cevt, timer;
};

/* 
 * As the system is currently structured (struct cos_sched_data_area
 * <= PAGE_SIZE), we can have a max of floor((PAGE_SIZE -
 * sizeof(struct cos_sched_next_thd) - sizeof(struct
 * cos_synchronization_atom) - sizeof(struct
 * cos_event_notification))/sizeof(struct cos_sched_events)) items in
 * the cos_events array, we are also limited by the size of "next" in
 * the cos_se_values, which in this case limits us to 256.
 */
#define NUM_SCHED_EVTS 128 //256

struct cos_sched_data_area {
	struct cos_sched_next_thd cos_next;
	union cos_synchronization_atom cos_locks;
	struct cos_event_notification cos_evt_notif;
	struct cos_sched_events cos_events[NUM_SCHED_EVTS]; // maximum of PAGE_SIZE/sizeof(struct cos_sched_events) - ceil(sizeof(struct cos_sched_curr_thd)/(sizeof(struct cos_sched_events)+sizeof(locks)))
} __attribute__((packed,aligned(4096)));

PERCPU_DECL(struct cos_sched_data_area, cos_sched_notifications);

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

#define XMIT_HEADERS_GATHER_LEN 32 
struct gather_item {
	void *data;
	int len;
};
struct cos_net_xmit_headers {
	/* Length of the header */
	int len, gather_len;
	/* Max IP header len + max TCP header len */
	char headers[80];
	struct gather_item gather_list[XMIT_HEADERS_GATHER_LEN];
}__attribute__((aligned(4096)));

enum {
	COS_BM_XMIT,
	COS_BM_XMIT_REGION,
	COS_BM_RECV_RING
};

/*
 * For interoperability with the networking side.  This is the acap
 * port/acap thread pair, and the callback structures for
 * communication.
 */
/* Added ring_buf pointers. */
struct cos_net_acap_info {
	unsigned short int  acap_port;
	struct async_cap   *acap;
	void               *private;

	/* HACK: recv ring buffer for network packets, both user-level
	 * and kernel-level pointers  */
	ring_buff_t *u_rb, *k_rb;
	int rb_next; 		/* Next address entry */
};

typedef void (*cos_net_data_completion_t)(void *data);
struct cos_net_callbacks {
	int (*xmit_packet)(void *headers, int hlen, struct gather_item *gi, int gather_len, int tot_len);
	int (*create_acap)(struct cos_net_acap_info *bi);
	int (*remove_acap)(struct cos_net_acap_info *bi);

	/* depricated: */
	int (*get_packet)(struct cos_net_acap_info *bi, char **packet, unsigned long *len,
			  cos_net_data_completion_t *fn, void **data, unsigned short int *port);
};

/* Communication of callback functions for the translator module */
struct cos_trans_fns {
	int   (*levt)(int channel);
	int   (*direction)(int direction);
	void *(*map_kaddr)(int channel);
	int   (*map_sz)(int channel);
	int   (*acap_created)(int channel, void *acap);
};

/*
 * These types are for addresses that are never meant to be
 * dereferenced.  They will generally be used to set up page table
 * entries.
 */
typedef unsigned long paddr_t;	/* physical address */
typedef unsigned long vaddr_t;	/* virtual address */
typedef unsigned int page_index_t;

typedef unsigned short int spdid_t;
typedef unsigned short int thdid_t;

struct restartable_atomic_sequence {
	vaddr_t start, end;
};

/* see explanation in spd.h */
struct usr_inv_cap {
	vaddr_t invocation_fn, service_entry_inst;
	unsigned int invocation_count, cap_no;
} __attribute__((aligned(16))); 

#define COMP_INFO_POLY_NUM 10
#define COMP_INFO_INIT_STR_LEN 128
#define COMP_INFO_STACK_FREELISTS 1

enum {
	COMP_INFO_TMEM_STK = 0,
	COMP_INFO_TMEM_CBUF,
	COMP_INFO_TMEM
};

/* Each stack freelist is associated with a thread id that can be used
 * by the assembly entry routines into a component to decide which
 * freelist to use. */
struct cos_stack_freelists {
	struct stack_fl {
		vaddr_t freelist;
		unsigned long thd_id;
	} freelists[COMP_INFO_STACK_FREELISTS];
};

/* move this to the stack manager assembly file, and use the ASM_... to access the relinquish variable */
//#define ASM_OFFSET_TO_STK_RELINQ (sizeof(struct cos_stack_freelists) + sizeof(u32_t) * COMP_INFO_TMEM_STK_RELINQ)
//#define ASM_OFFSET_TO_STK_RELINQ 8
/* #ifdef COMP_INFO_STACK_FREELISTS != 1 || COMP_INFO_TMEM_STK_RELINQ != 0 */
/* #error "Assembly in <fill in file name here> requires that COMP_INFO_STACK_FREELISTS != 1 || COMP_INFO_TMEM_STK_RELINQ != 0.  Change the defines, or change the assembly" */
/* #endif */

struct cos_component_information {
	struct cos_stack_freelists cos_stacks;
	long cos_this_spd_id;
	u32_t cos_tmem_relinquish[COMP_INFO_TMEM];
	u32_t cos_tmem_available[COMP_INFO_TMEM];
	vaddr_t cos_heap_ptr, cos_heap_limit;
	vaddr_t cos_heap_allocated, cos_heap_alloc_extent;
	vaddr_t cos_upcall_entry;
	vaddr_t cos_async_inv_entry;
//	struct cos_sched_data_area *cos_sched_data_area;
	vaddr_t cos_user_caps;
	struct restartable_atomic_sequence cos_ras[COS_NUM_ATOMIC_SECTIONS/2];
	vaddr_t cos_poly[COMP_INFO_POLY_NUM];
	char init_string[COMP_INFO_INIT_STR_LEN];
}__attribute__((aligned(PAGE_SIZE)));

typedef enum {
	COS_UPCALL_ACAP_COMPLETE,
	COS_UPCALL_THD_CREATE,
	COS_UPCALL_DESTROY,
	COS_UPCALL_UNHANDLED_FAULT
} upcall_type_t;

/* operations for cos_thd_cntl */
enum {
	COS_THD_INV_FRAME, 	/* Get the ith invocation frame for the thread */
	COS_THD_INV_FRAME_REM, 	/* Remove a component return at an offset into the thd's stack */
	COS_THD_INV_SPD,        /* has the spd been invoked by the thread? return offset into invstk */
	COS_THD_INVFRM_IP,	/* get the instruction pointer in an inv frame  */
	COS_THD_INVFRM_SET_IP,
	COS_THD_INVFRM_SP,	/* get the stack pointer in an inv frame  */
	COS_THD_INVFRM_SET_SP,
	/* 
	 * For the following GET methods, the argument is 0 to get the
	 * register of a _preempted thread_, or 1 to get the fault
	 * register of the thread.  If the thread is not preempted and
	 * arg1==0, return 0
	 */
	COS_THD_GET_IP,
	COS_THD_GET_SP,
	COS_THD_GET_FP,
	COS_THD_GET_1,
	COS_THD_GET_2,
	COS_THD_GET_3,
	COS_THD_GET_4,
	COS_THD_GET_5,
	COS_THD_GET_6,

	/* 
	 * For the following SET methods, arg1 is the value to set the
	 * register to, and arg2 is 0 if we wish to set the register
	 * for a preempted thread, while it is 1 if we wish to set the
	 * fault registers for the thread.  Return -1, and do nothing
	 * if arg2 == 0, and the thread is not preempted.
	 */
	COS_THD_SET_IP,
	COS_THD_SET_SP,
	COS_THD_SET_FP,
	COS_THD_SET_1,
	COS_THD_SET_2,
	COS_THD_SET_3,
	COS_THD_SET_4,
	COS_THD_SET_5,
	COS_THD_SET_6,

	COS_THD_STATUS
};

/* operations for cos_spd_cntl */
enum {
	COS_SPD_CREATE,
	COS_SPD_DELETE,
	COS_SPD_LOCATION,
	COS_SPD_ATOMIC_SECT,
	COS_SPD_UCAP_TBL,
	COS_SPD_UPCALL_ADDR,
	COS_SPD_ASYNC_INV_ADDR,
	COS_SPD_ACTIVATE,
	COS_SPD_INC_FORK_CNT
};

/* operations for cos_vas_cntl */
enum {
	COS_VAS_CREATE, 	/* new vas */
	COS_VAS_DELETE,		/* remove vas */
	COS_VAS_SPD_ADD,	/* add spd to vas */
	COS_VAS_SPD_REM,	/* remove spd from vas */
	COS_VAS_SPD_EXPAND,	/* allocate more vas to spd */
	COS_VAS_SPD_RETRACT	/* deallocate some vas from spd */
};

enum {
	COS_CAP_SET_CSTUB,
	COS_CAP_SET_SSTUB,
	COS_CAP_SET_SERV_FN,
	COS_CAP_ACTIVATE,
	COS_CAP_GET_INVCNT,
	COS_CAP_SET_FAULT,
	COS_CAP_GET_SPD_NCAPS,
	COS_CAP_GET_DEST_SPD,
	COS_CAP_GET_DEST_FN,
	COS_CAP_INC_FORK_CNT
};

enum {
	COS_HW_TIMER,
	COS_HW_NET,
	COS_UC_NOTIF
};

/* operations for cos_sched_cntl */
enum {
	COS_SCHED_EVT_REGION,
	COS_SCHED_THD_EVT,
	COS_SCHED_PROMOTE_CHLD,
	COS_SCHED_PROMOTE_ROOT,
	COS_SCHED_GRANT_SCHED,
	COS_SCHED_REVOKE_SCHED,
	COS_SCHED_REMOVE_THD,
	COS_SCHED_BREAK_PREEMPTION_CHAIN
};

enum {
	COS_TRANS_DIR_INVAL = 0,
	COS_TRANS_DIR_LTOC,
	COS_TRANS_DIR_CTOL,
};


enum {
	COS_TRANS_SERVICE_PRINT   = 0,
	COS_TRANS_SERVICE_TERM,
	COS_TRANS_SERVICE_PING,
	COS_TRANS_SERVICE_PONG,
	COS_TRANS_SERVICE_MAX     = 10
};

enum {
	COS_TRANS_TRIGGER,
	COS_TRANS_MAP_SZ,
	COS_TRANS_MAP,
	COS_TRANS_DIRECTION,
	COS_TRANS_ACAP,
};

/* operations for cos_async_cap_cntl */
enum {
	COS_ACAP_CREATE = 0,
	COS_ACAP_CLI_CREATE,
	COS_ACAP_SRV_CREATE,
	COS_ACAP_WIRE,
	COS_ACAP_LINK_STATIC_CAP,
};

/* flags for cos_switch_thread */
#define COS_SCHED_TAILCALL     0x1
#define COS_SCHED_SYNC_BLOCK   0x2
#define COS_SCHED_SYNC_UNBLOCK 0x4
#define COS_SCHED_ACAP_WAIT   0x80
#define COS_SCHED_CHILD_EVT    0x10

#define COS_SCHED_RET_SUCCESS  0
#define COS_SCHED_RET_ERROR    (-1)
/* Referenced a resource (tid) that is not valid */
#define COS_SCHED_RET_INVAL    (-2)
/* Either we tried to schedule ourselves, or an event occurred that we
 * haven't processed: do scheduling computations again! */
#define COS_SCHED_RET_AGAIN    1
#define COS_SCHED_RET_CEVT     2

struct mpd_split_ret {
	short int new, old;
} __attribute__((packed));

static inline int mpd_split_error(struct mpd_split_ret ret)
{
	return (ret.new < 0) ? 1 : 0;
}

/* operations for manipulating mpds */
enum {
	COS_MPD_SPLIT, 		/* split an spd out of an cspd */
	COS_MPD_MERGE,		/* merge two cspds */
	COS_MPD_DEACTIVATE,	/* deactivate a cspd (set its page
				 * table to 0), so that it won't be
				 * used, causing mpd faults
				 * instead */
	COS_MPD_REAP,		/* return the id of and free a cspd
				 * that has no more references to
				 * it */
	COS_MPD_UPDATE		/* if possible, get rid of a stale pd
				 * for the current thread. */
};

enum {
	MAPPING_READ  = 0,
	MAPPING_RW    = 1
};

enum {
	COS_MMAP_GRANT,
	COS_MMAP_REVOKE,
	COS_MMAP_TLBFLUSH
};

enum {
	COS_PFN_GRANT,
	COS_PFN_MAX_MEM
};

/* 
 * Fault and fault handler information.  Fault indices/identifiers and
 * the function names to handle them.
 */
typedef enum {
	COS_FLT_PGFLT,
	COS_FLT_DIVZERO,
	COS_FLT_BRKPT,
	COS_FLT_OVERFLOW,
	COS_FLT_RANGE,
	COS_FLT_GEN_PROT,
	/* software defined: */
	COS_FLT_LINUX,
	COS_FLT_SAVE_REGS,
	COS_FLT_FLT_NOTIF,
	COS_FLT_QUARANTINE,
	COS_FLT_MAX
} cos_flt_off; /* <- this indexes into cos_flt_handlers in the loader */

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

#define LOCK_PREFIX_HERE			\
	".pushsection .smp_locks,\"a\"\n"	\
	".balign 4\n"				\
	".long 671f - .\n" /* offset */		\
	".popsection\n"				\
	"671:"

#define LOCK_PREFIX LOCK_PREFIX_HERE "\n\tlock; "

static inline void atomic_inc(atomic_t *v)
{
	asm volatile(LOCK_PREFIX "incl %0"
		     : "+m" (v->counter));
}

static inline void atomic_dec(atomic_t *v)
{
	asm volatile(LOCK_PREFIX "decl %0"
		     : "+m" (v->counter));
}
#endif /* __KERNEL__ */

static inline void cos_ref_take(atomic_t *rc)
{
#if NUM_CPU_COS > 1
	/* use atomic instructions when we have multicore. We will get
	 * rid of this later(by using capabilities as ref counter). */
	atomic_inc(rc);
#else
	rc->counter++;
#endif
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
#if NUM_CPU_COS > 1
	/* same as cos_ref_take. */
	atomic_dec(rc);
#else
	rc->counter--; /* assert(rc->counter != 0) */
#endif
	
	cos_meas_event(COS_MPD_REFCNT_DEC);
}

// The init_data is integrated in the sched_param struct. We have 8 bits
#define COS_THD_INIT_REGION_SIZE (((NUM_CPU*8) > (1<<8)) ? (1<<8) : (NUM_CPU*8))
// Static entries are after the dynamic allocated entries
#define COS_STATIC_THD_ENTRY(i) ((i + COS_THD_INIT_REGION_SIZE + 1))

#endif /* TYPES_H */
