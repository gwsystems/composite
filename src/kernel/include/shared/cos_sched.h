#ifndef COS_SCHED_H
#define COS_SCHED_H

#include "./cos_types.h"

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

struct cos_dcb_info {
	unsigned long ip;
	unsigned long sp;
	unsigned long pending; /* binary value. TODO: move it to ip or sp */
	unsigned long vas_id;
} __attribute__((packed));

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
#define DCB_IP_KERN_OFF 8

#if defined(__x86_64__)
#define SLITE_BIT_PACK_SZ 32
#else
#define SLITE_BIT_PACK_SZ 16
#endif

#endif /* COS_SCHED_H */
