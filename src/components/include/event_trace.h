#ifndef EVENT_TRACE_H
#define EVENT_TRACE_H

#define EVENT_TRACE_START_MAGIC 0xdeadbeef

enum {
	SYSCALL_EVENT,
	SL_EVENT,

	QUEUE_EVENT,
	MUTEX_EVENT,
	BINSEM_EVENT,
	COUNTSEM_EVENT,
	FILE_SYS_EVENT,
};

enum syscall_event_type {
	SYSCALL_THD_SWITCH_START,
	SYSCALL_THD_SWITCH_END,

	SYSCALL_RCV_START,
	SYSCALL_RCV_END,

	SYSCALL_SCHED_RCV_START,
	SYSCALL_SCHED_RCV_END,

	SYSCALL_ASND_START,
	SYSCALL_ASND_END,

	SYSCALL_SCHED_ASND_START,
	SYSCALL_SCHED_ASND_END,

	/* TODO: remaining cos_kernel_api */
};

enum sl_event_type {
	SL_BLOCK_START,
	SL_BLOCK_END,
	SL_BLOCK_TIMEOUT_START,
	SL_BLOCK_TIMEOUT_END,

	SL_YIELD_START,
	SL_YIELD_END,

	SL_WAKEUP_START,
	SL_WAKEUP_END,
};

enum queue_event_type {
	QUEUE_ENQUEUE_START,
	QUEUE_ENQUEUE_END,

	QUEUE_DEQUEUE_START,
	QUEUE_DEQUEUE_END,
};

enum mutex_event_type {
	MUTEX_TAKE_START,
	MUTEX_TAKE_END,

	MUTEX_GIVE_START,
	MUTEX_GIVE_END,
};

enum binsem_event_type {
	BINSEM_TAKE_START,
	BINSEM_TAKE_END,

	BINSEM_GIVE_START,
	BINSEM_GIVE_END,

	BINSEM_TIMEDWAIT_START,
	BINSEM_TIMEDWAIT_END,
};

enum countsem_event_type {
	COUNTSEM_TAKE_START,
	COUNTSEM_TAKE_END,

	COUNTSEM_GIVE_START,
	COUNTSEM_GIVE_END,

	COUNTSEM_TIMEDWAIT_START,
	COUNTSEM_TIMEDWAIT_END,
};

struct event_trace_info {
	unsigned short type;
	unsigned short sub_type;

	unsigned long long ts;

	unsigned short thdid;
	unsigned short objid; /* TODO */
};

#ifndef LINUX_DECODE
#include <ck_ring.h>

CK_RING_PROTOTYPE(evttrace, event_trace_info);
#endif

void event_trace_init(void);
/*
 *@return 0 - successful, 1 - failed to trace event
 */
int event_trace(struct event_trace_info *ei);
void event_decode(void *trace, int sz);

/*
 * TODO:
 * 1. optimized API to allow compiler optimizations.
 * 2. macro API so the tracing feature can be disabled.
 */
#define EVTTR_SYSCALL_THDSW_START(dst) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = SYSCALL_EVENT;		\
				ei.sub_type = SYSCALL_THD_SWITCH_START;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = dst;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_SYSCALL_THDSW_END() 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = SYSCALL_EVENT;		\
				ei.sub_type = SYSCALL_THD_SWITCH_END;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = 0;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_SL_BLOCK_START(dst) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = SL_EVENT;			\
				ei.sub_type = SL_BLOCK_START;		\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = dst;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_SL_BLOCK_END()	 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = SL_EVENT;			\
				ei.sub_type = SL_BLOCK_END;		\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = 0;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_SL_BLOCK_TIMEOUT_START(dst)				\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = SL_EVENT;			\
				ei.sub_type = SL_BLOCK_TIMEOUT_START;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = dst;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_SL_BLOCK_TIMEOUT_END() 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = SL_EVENT;			\
				ei.sub_type = SL_BLOCK_TIMEOUT_END;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = 0;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_SL_WAKEUP_START(dst) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = SL_EVENT;			\
				ei.sub_type = SL_WAKEUP_START;		\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = dst;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_SL_WAKEUP_END()	 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = SL_EVENT;			\
				ei.sub_type = SL_WAKEUP_END;		\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = 0;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_SL_YIELD_START(dst) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = SL_EVENT;			\
				ei.sub_type = SL_YIELD_START;		\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = dst;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_SL_YIELD_END()	 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = SL_EVENT;			\
				ei.sub_type = SL_YIELD_END;		\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = 0;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_QUEUE_ENQ_START(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = QUEUE_EVENT;			\
				ei.sub_type = QUEUE_ENQUEUE_START;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_QUEUE_ENQ_END(obj)	 				\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = QUEUE_EVENT;			\
				ei.sub_type = QUEUE_ENQUEUE_END;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_QUEUE_DEQ_START(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = QUEUE_EVENT;			\
				ei.sub_type = QUEUE_DEQUEUE_START;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_QUEUE_DEQ_END(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = QUEUE_EVENT;			\
				ei.sub_type = QUEUE_DEQUEUE_END;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_MUTEX_TAKE_START(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = MUTEX_EVENT;			\
				ei.sub_type = MUTEX_TAKE_START;		\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_MUTEX_TAKE_END(obj)	 				\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = MUTEX_EVENT;			\
				ei.sub_type = MUTEX_TAKE_END;		\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_MUTEX_GIVE_START(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = MUTEX_EVENT;			\
				ei.sub_type = MUTEX_GIVE_START;		\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_MUTEX_GIVE_END(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = MUTEX_EVENT;			\
				ei.sub_type = MUTEX_GIVE_END;		\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_BINSEM_TAKE_START(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = BINSEM_EVENT;			\
				ei.sub_type = BINSEM_TAKE_START;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_BINSEM_TAKE_END(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = BINSEM_EVENT;			\
				ei.sub_type = BINSEM_TAKE_END;		\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_BINSEM_GIVE_START(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = BINSEM_EVENT;			\
				ei.sub_type = BINSEM_GIVE_START;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_BINSEM_GIVE_END(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = BINSEM_EVENT;			\
				ei.sub_type = BINSEM_GIVE_END;		\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_BINSEM_TIMEDWAIT_START(obj)				\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = BINSEM_EVENT;			\
				ei.sub_type = BINSEM_TIMEDWAIT_START;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_BINSEM_TIMEDWAIT_END(obj)					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = BINSEM_EVENT;			\
				ei.sub_type = BINSEM_TIMEDWAIT_END;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_COUNTSEM_TAKE_START(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = COUNTSEM_EVENT;		\
				ei.sub_type = COUNTSEM_TAKE_START;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_COUNTSEM_TAKE_END(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = COUNTSEM_EVENT;		\
				ei.sub_type = COUNTSEM_TAKE_END;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_COUNTSEM_GIVE_START(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = COUNTSEM_EVENT;		\
				ei.sub_type = COUNTSEM_GIVE_START;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_COUNTSEM_GIVE_END(obj) 					\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = COUNTSEM_EVENT;		\
				ei.sub_type = COUNTSEM_GIVE_END;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_COUNTSEM_TIMEDWAIT_START(obj)				\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = COUNTSEM_EVENT;		\
				ei.sub_type = COUNTSEM_TIMEDWAIT_START;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)

#define EVTTR_COUNTSEM_TIMEDWAIT_END(obj)				\
			do {						\
				struct event_trace_info ei;		\
									\
				ei.type = COUNTSEM_EVENT;		\
				ei.sub_type = COUNTSEM_TIMEDWAIT_END;	\
				rdtscll(ei.ts);				\
				ei.thdid = cos_thdid();			\
				ei.objid = obj;				\
									\
				event_trace(&ei);			\
			} while (0)


#endif /* EVENT_TRACE_H */
