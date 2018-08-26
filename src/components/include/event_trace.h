//#ifndef EVENT_LINUX_DECODE
//#ifndef EVENT_TRACE_ENABLE
//#define EVENT_TRACE_ENABLE
//#endif
//#endif

#ifndef EVENT_TRACE_H
#define EVENT_TRACE_H

#define EVENT_TRACE_START_MAGIC 0xdeadbeef
#define EVENT_TRACE_REMOTE
#define EVENT_TRACE_INPORT 10205
#define EVENT_TRACE_OUTPORT 10206
#define EVENT_TRACE_HOSTIP  "192.168.0.1"

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

	unsigned short thdid;
	unsigned short objid;

	unsigned long long ts;

	/* keep this struct a power of 2, if required by padding. */
};

void event_decode(void *trace, int sz);

#ifndef EVENT_LINUX_DECODE

#include <ck_ring.h>
#include <cos_component.h>
#include <cos_kernel_api.h>

CK_RING_PROTOTYPE(evttrace, event_trace_info);

#ifndef EVENT_TRACE_REMOTE
void event_trace_init(void);
#else

#define EVENT_TRACE_KEY 0xdead
#define EVENT_TRACE_NPAGES 16

typedef int (*evttrace_write_fn_t)(unsigned char *buf, unsigned int bufsz);

/* server that is producing events. */
void event_trace_server_init(void);
/* remote client that is consuming events. */
void event_trace_client_init(evttrace_write_fn_t fn);

#endif
/*
 *@return 0 - successful, 1 - failed to trace event
 */
int event_trace(struct event_trace_info *ei);

int event_flush(void);

static inline void
event_info_trace(unsigned short type, unsigned short sub_type, unsigned short objid)
{
#ifdef EVENT_TRACE_ENABLE
	struct event_trace_info ei = { .type = type, .sub_type = sub_type, .thdid = cos_thdid(), .objid = objid };

	rdtscll(ei.ts);

	event_trace(&ei);
#endif
}

#define EVTTR_SYSCALL_THDSW_START(dst)      event_info_trace(SYSCALL_EVENT, SYSCALL_THD_SWITCH_START, dst)
#define EVTTR_SYSCALL_THDSW_END()           event_info_trace(SYSCALL_EVENT, SYSCALL_THD_SWITCH_END, 0)

#define EVTTR_SL_BLOCK_START(dst)           event_info_trace(SL_EVENT, SL_BLOCK_START, dst)
#define EVTTR_SL_BLOCK_END()                event_info_trace(SL_EVENT, SL_BLOCK_END, 0)
#define EVTTR_SL_BLOCK_TIMEOUT_START(dst)   event_info_trace(SL_EVENT, SL_BLOCK_TIMEOUT_START, dst)
#define EVTTR_SL_BLOCK_TIMEOUT_END()        event_info_trace(SL_EVENT, SL_BLOCK_TIMEOUT_END, 0)
#define EVTTR_SL_WAKEUP_START(dst)          event_info_trace(SL_EVENT, SL_WAKEUP_START, dst)
#define EVTTR_SL_WAKEUP_END()               event_info_trace(SL_EVENT, SL_WAKEUP_END, 0)
#define EVTTR_SL_YIELD_START(dst)           event_info_trace(SL_EVENT, SL_YIELD_START, dst)
#define EVTTR_SL_YIELD_END()                event_info_trace(SL_EVENT, SL_YIELD_END, 0)

#define EVTTR_QUEUE_ENQ_START(obj)          event_info_trace(QUEUE_EVENT, QUEUE_ENQUEUE_START, obj)
#define EVTTR_QUEUE_ENQ_END(obj)            event_info_trace(QUEUE_EVENT, QUEUE_ENQUEUE_END, obj)
#define EVTTR_QUEUE_DEQ_START(obj)          event_info_trace(QUEUE_EVENT, QUEUE_DEQUEUE_START, obj)
#define EVTTR_QUEUE_DEQ_END(obj)            event_info_trace(QUEUE_EVENT, QUEUE_DEQUEUE_END, obj)

#define EVTTR_MUTEX_TAKE_START(obj)         event_info_trace(MUTEX_EVENT, MUTEX_TAKE_START, obj)
#define EVTTR_MUTEX_TAKE_END(obj)           event_info_trace(MUTEX_EVENT, MUTEX_TAKE_END, obj)
#define EVTTR_MUTEX_GIVE_START(obj)         event_info_trace(MUTEX_EVENT, MUTEX_GIVE_START, obj)
#define EVTTR_MUTEX_GIVE_END(obj)           event_info_trace(MUTEX_EVENT, MUTEX_GIVE_END, obj)

#define EVTTR_BINSEM_TAKE_START(obj)        event_info_trace(BINSEM_EVENT, BINSEM_TAKE_START, obj)
#define EVTTR_BINSEM_TAKE_END(obj)          event_info_trace(BINSEM_EVENT, BINSEM_TAKE_END, obj)
#define EVTTR_BINSEM_GIVE_START(obj)        event_info_trace(BINSEM_EVENT, BINSEM_GIVE_START, obj)
#define EVTTR_BINSEM_GIVE_END(obj)          event_info_trace(BINSEM_EVENT, BINSEM_GIVE_END, obj)
#define EVTTR_BINSEM_TIMEDWAIT_START(obj)   event_info_trace(BINSEM_EVENT, BINSEM_TIMEDWAIT_START, obj)
#define EVTTR_BINSEM_TIMEDWAIT_END(obj)     event_info_trace(BINSEM_EVENT, BINSEM_TIMEDWAIT_END, obj)

#define EVTTR_COUNTSEM_TAKE_START(obj)      event_info_trace(COUNTSEM_EVENT, COUNTSEM_TAKE_START, obj)
#define EVTTR_COUNTSEM_TAKE_END(obj)        event_info_trace(COUNTSEM_EVENT, COUNTSEM_TAKE_END, obj)
#define EVTTR_COUNTSEM_GIVE_START(obj)      event_info_trace(COUNTSEM_EVENT, COUNTSEM_GIVE_START, obj)
#define EVTTR_COUNTSEM_GIVE_END(obj)        event_info_trace(COUNTSEM_EVENT, COUNTSEM_GIVE_END, obj)
#define EVTTR_COUNTSEM_TIMEDWAIT_START(obj) event_info_trace(COUNTSEM_EVENT, COUNTSEM_TIMEDWAIT_START, obj)
#define EVTTR_COUNTSEM_TIMEDWAIT_END(obj)   event_info_trace(COUNTSEM_EVENT, COUNTSEM_TIMEDWAIT_END, obj)
#else
void *event_trace_check_hdr(void *buf, unsigned int *sz);
#endif

#endif /* EVENT_TRACE_H */
