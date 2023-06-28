#ifndef COS_SCHED_H
#define COS_SCHED_H

#include "./cos_types.h"

#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

struct cos_thd_event {
	u16_t         blocked;
	u32_t         next_timeout;
	u64_t         elapsed_cycs;
} __attribute__((packed));

struct cos_sched_event {
	thdid_t tid;
	struct cos_thd_event evt;
} __attribute__((packed));

#define COS_SCHED_EVENT_RING_SIZE 16

struct cos_sched_ring {
	int head, tail;
	struct cos_sched_event event_buf[COS_SCHED_EVENT_RING_SIZE];
} __attribute__((packed));

/* FIXME: priority... */
struct cos_scb_info {
	thdid_t               tid;
	capid_t               curr_thd;
	tcap_time_t           timer_pre;
	sched_tok_t           sched_tok;
	struct cos_sched_ring sched_events;
} CACHE_ALIGNED;

COS_STATIC_ASSERT(COS_SCB_INFO_SIZE == sizeof(struct cos_scb_info), "Update COS_SCB_INFO_SIZE with sizeof struct cos_scb_info");
COS_STATIC_ASSERT(COS_SCB_THDCAP_OFFSET == offsetof(struct cos_scb_info, curr_thd), "Update COS_SCB_THDCAP_OFFSET with the offset of curr_thd in the SCB");
COS_STATIC_ASSERT(COS_SCB_TID_OFFSET == offsetof(struct cos_scb_info, tid), "Update COS_SCB_TID_OFFSET with the offset of tid in the SCB");

struct cos_dcb_info {
	unsigned long ip;
	unsigned long sp;
	unsigned long pending; /* binary value. TODO: move it to ip or sp */
	unsigned long vas_id;
} __attribute__((packed));

COS_STATIC_ASSERT(COS_DCB_INFO_SIZE == sizeof(struct cos_dcb_info), "Update COS_DCB_INFO_SIZE with sizeof struct cos_scb_info");
COS_STATIC_ASSERT(COS_DCB_IP_OFFSET == offsetof(struct cos_dcb_info, ip), "Update COS_DCB_IP_OFFSET with the offset of ip in the dcb");
COS_STATIC_ASSERT(COS_DCB_SP_OFFSET == offsetof(struct cos_dcb_info, sp), "Update COS_DCB_SP_OFFSET with the offset of sp in the dcb");

/*
 * This is the "ip" the kernel uses to update the thread when it sees that the
 * thread is still in user-level dispatch routine.
 * This is the offset of instruction after resetting the "next" thread's "sp" to zero
 * in a purely user-level dispatch.
 *
 * Whenever kernel is switching to a thread which has "sp" non-zero, it would switch
 * to the "ip" saved in the dcb_info and reset the "sp" of the thread that the kernel
 * is dispatching to!
 * This is necessary because, if the kernel is dispatching to a thread that was in the
 * user-level dispatch routine before, then the only registers that it can restore are
 * "ip" and "sp", everything else is either clobbered or saved/loaded at user-level.
 */
#if defined (__PROTECTED_DISPATCH__)
#define DCB_IP_KERN_OFF 72
#else
#define DCB_IP_KERN_OFF 8
#endif

#if defined(__x86_64__)
#define SLITE_BIT_PACK_SZ 32
#else
#define SLITE_BIT_PACK_SZ 16
#endif

#endif /* COS_SCHED_H */
