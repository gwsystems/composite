#include <event_trace.h>

const char *syscall_msg[] = {
	"sys: thread switch start, switch to=%u\n",
	"sys: thread switch end\n",
	"sys: receive start\n",
	"sys: receive end\n",
	"sys: scheduler receive start\n",
	"sys: scheduler receive end\n",
	"sys: asynchronous send start\n",
	"sys: asynchronous send end\n",
	"sys: scheduler asynchronous send start\n",
	"sys: scheduler asynchronous send end\n",
};

const char *sl_msg[] = {
	"sl: block start, dependency=%u\n",
	"sl: block end\n",
	"sl: timed block start, dependency=%u\n", //could specify timeout here.
	"sl: timed block end\n",
	"sl: yield start, directed=%u\n",
	"sl: yield end\n",
	"sl: wakeup start, wakeup=%u\n",
	"sl: wakeup end\n",
};

const char *queue_msg[] = {
	"queue: id=%u enqueue start\n",
	"queue: id=%u enqueue end\n",
	"queue: id=%u dequeue start\n",
	"queue: id=%u dequeue end\n",
};

const char *mutex_msg[] = {
	"mutex: id=%u take start\n",
	"mutex: id=%u take end\n",
	"mutex: id=%u give start\n",
	"mutex: id=%u give end\n",
};

const char *binsem_msg[] = {
	"binsem: id=%u take start\n",
	"binsem: id=%u take end\n",
	"binsem: id=%u give start\n",
	"binsem: id=%u give end\n",
	"binsem: id=%u timedwait start\n",
	"binsem: id=%u timedwait end\n",
};

const char *countsem_msg[] = {
	"countsem: id=%u take start\n",
	"countsem: id=%u take end\n",
	"countsem: id=%u give start\n",
	"countsem: id=%u give end\n",
	"countsem: id=%u timedwait start\n",
	"countsem: id=%u timedwait end\n",
};

#define EVTTRACE_HDR_SZ (sizeof(unsigned int) * 2 + sizeof(unsigned long long))
#define EVTTRACE_MSG_LEN 128

#ifndef EVENT_LINUX_DECODE

#include <ck_ring.h>

#undef EVTTRACE_BATCH_OUTPUT
#undef EVTTRACE_DEBUG_TRACE
#define EVTTRACE_RETRY_MAX 10
/* skipped: how many failed retry. queued: how many were written to ring buffer. logged: how many were written to serial output */
static unsigned long long skipped = 0, queued = 0, logged = 0;
static int evttrace_initialized = 0;
static unsigned long long evttrace_st_tsc = 0;
static unsigned int evttrace_cpu_cycs_usec = 0;
static unsigned char tracehdr[EVTTRACE_HDR_SZ] = { 0 };

#ifndef EVENT_TRACE_REMOTE

#include <cos_serial.h>
#include <sl_lock.h>

#define EVTTRACE_RING_SIZE 512
#define EVTTRACE_RINGBUF_SIZE (sizeof(struct event_trace_info) * EVTTRACE_RING_SIZE)
#define EVTTRACE_BATCH_SIZE 128
#define EVTTRACE_BATCHBUF_SIZE (sizeof(struct event_trace_info) * EVTTRACE_BATCH_SIZE)
/*
 * FIXME:
 * For now, tracing only in this component.
 */
static struct ck_ring evttrace_ring;
static struct event_trace_info evttrace_buf[EVTTRACE_RING_SIZE];

void
event_trace_init(void)
{
#ifdef EVENT_TRACE_ENABLE
	unsigned char tracehdr[EVTTRACE_HDR_SZ] = { 0 };
	unsigned int magic = EVENT_TRACE_START_MAGIC;
	unsigned int cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	unsigned long long st_tsc = 0;

	PRINTC("Event trace initialization!\n");
	memset(evttrace_buf, 0, EVTTRACE_RINGBUF_SIZE);
	ck_ring_init(&evttrace_ring, EVTTRACE_RING_SIZE);
	rdtscll(st_tsc);
	memcpy(tracehdr, &magic, sizeof(unsigned int));
	memcpy(tracehdr + sizeof(unsigned int), &cycs, sizeof(unsigned int));
	memcpy(tracehdr + (sizeof(unsigned int) * 2), &st_tsc, sizeof(unsigned long long));
	evttrace_st_tsc = st_tsc;
	evttrace_cpu_cycs_usec = cycs;

	serial_print((void *)&tracehdr, EVTTRACE_HDR_SZ);

	evttrace_initialized = 1;
#endif
}

int
event_flush(void)
{
	unsigned char flush_buf[EVTTRACE_BATCHBUF_SIZE] = { 0 };
	unsigned int count = 0, ret_count = 0, batch_sz = 0;

	if (unlikely(evttrace_initialized == 0)) return 0;

	batch_sz = ck_ring_size(&evttrace_ring);

	batch_sz = batch_sz > EVTTRACE_BATCH_SIZE ? EVTTRACE_BATCH_SIZE : batch_sz;

	while (count < batch_sz) {
		struct event_trace_info evtinfo;
		int retry_count = 0, ret;

retry:
		ret = ck_ring_trydequeue_mpmc_evttrace(&evttrace_ring, evttrace_buf, &evtinfo);

		/* as long as there is data in the queue */
		if (ret != true) {
			retry_count++;

			if (likely(retry_count < EVTTRACE_RETRY_MAX)) goto retry;

			break;
		}

#ifdef EVTTRACE_BATCH_OUTPUT
		memcpy(flush_buf + (sizeof(struct event_trace_info) * count), &evtinfo, sizeof(struct event_trace_info));
#else
		serial_print((void *)&evtinfo, sizeof(struct event_trace_info));
#endif
		count++;
	}

	ret_count = count;

#if 0
//#ifdef EVTTRACE_BATCH_OUTPUT
	batch_sz = LLPRINT_SERIAL_MAX_LEN / sizeof(struct event_trace_info);

	count = 0;
	while (count < ret_count) {
		int len = ret_count - count;

		len = len > batch_sz ? batch_sz : len;
		serial_print(flush_buf + (sizeof(struct event_trace_info) * count), sizeof(struct event_trace_info) * len);

		count += len;
	}
#endif
	logged += ret_count;

	return ret_count;
}

int
event_trace(struct event_trace_info *ei)
{
	int count = 0;

	/* don't log yet or don't log for components that don't initialize, ex: sl events only for cFE..*/
	if (unlikely(evttrace_initialized == 0)) return 0;

retry:
	/* mpmc because any thread could enqueue events and dequeue(flush out to serial) */
	if (ck_ring_enqueue_mpmc_evttrace(&evttrace_ring, evttrace_buf, ei) != true) {
		event_flush();

		count++;
		/* TODO: perhaps spit out number of skipped msgs or write directly to serial or something?? */
		if (unlikely(count >= EVTTRACE_RETRY_MAX)) {
			skipped++;
			return -1;
		}

		goto retry;
	}

	queued++;
	if (unlikely(queued % 10000 == 0)) PRINTC("Skipped: %llu, Queued: %llu, Logged: %llu\n", skipped, queued, logged);

	return 0;
}

#else

#include <sl_lock.h>
#ifdef EVENT_TRACE_ENABLE
/* you should add channel and capmgr to your interface dependency list */
#include "../interface/channel/channel.h"
#endif

static cbuf_t evttrace_shmid = 0;
static volatile vaddr_t evttrace_vaddr = 0;
static volatile struct ck_ring *evttrace_ring = NULL;
static volatile struct event_trace_info *evttrace_buf = NULL;
static evttrace_write_fn_t evttrace_write_fn = NULL;
static volatile struct sl_lock evttrace_wlock = SL_LOCK_STATIC_INIT();

#define EVTTRACE_WLOCK_TAKE() (sl_lock_take(&evttrace_wlock))
#define EVTTRACE_WLOCK_GIVE() (sl_lock_release(&evttrace_wlock))

#define EVTTRACE_RING ((struct ck_ring *)evttrace_ring)
#define EVTTRACE_BUF ((struct event_trace_info *)evttrace_buf)

/* NPAGES must be = (power of 2) + 1 */
#define EVTTRACE_RINGBUF_SIZE ((EVENT_TRACE_NPAGES-1) * PAGE_SIZE)
#define EVTTRACE_RING_SIZE (EVTTRACE_RINGBUF_SIZE / sizeof(struct event_trace_info))

#define EVTTRACE_BATCHBUF_SIZE 1024 /* MTU - approx UDP header size */
#define EVTTRACE_BATCH_SIZE (EVTTRACE_BATCHBUF_SIZE / sizeof(struct event_trace_info))

#define EVTTRACE_SHM_ID(d)      (d)
#define EVTTRACE_SHM_CLIENT(d)  (d + sizeof(vaddr_t))
#define EVTTRACE_SHM_SERVER(d)  (d + (2 * sizeof(vaddr_t)))
#define EVTTRACE_SHM_RING(d)    (d + (3 * sizeof(vaddr_t)))
#define EVTTRACE_SHM_RINGBUF(d) (d + PAGE_SIZE)

void
event_trace_server_init(void)
{
#ifdef EVENT_TRACE_ENABLE
	unsigned long npages = 0;
	int deq_count = 0, ret;
	struct event_trace_info ei;

	/* wait for the client to create shm */
	while (!evttrace_shmid) evttrace_shmid = channel_shared_page_map(EVENT_TRACE_KEY, (vaddr_t *)&evttrace_vaddr, &npages);
	assert(evttrace_shmid && evttrace_vaddr && npages == EVENT_TRACE_NPAGES);
	/* wait for the client to init. */
	while (ps_load((unsigned long *)EVTTRACE_SHM_ID(evttrace_vaddr)) != evttrace_shmid) ;
	/* 1 page pretty much wasted! */
	evttrace_buf = (volatile struct event_trace_info *)EVTTRACE_SHM_RINGBUF(evttrace_vaddr);
	evttrace_ring = (volatile struct ck_ring *)EVTTRACE_SHM_RING(evttrace_vaddr);

	/* test max deq in ck rings */
	while (ck_ring_dequeue_mpsc_evttrace(EVTTRACE_RING, EVTTRACE_BUF, &ei) == true) {
		deq_count++;
	}
	assert(deq_count == EVTTRACE_RING_SIZE-1);

	PRINTC("Server init done! [capacity: %u][size:%u]\n", ck_ring_capacity(EVTTRACE_RING), ck_ring_size(EVTTRACE_RING));
	assert(ck_ring_capacity(EVTTRACE_RING) == EVTTRACE_RING_SIZE);
	ret = ps_cas((unsigned long *)EVTTRACE_SHM_SERVER(evttrace_vaddr), 0, 1);
	assert(ret == true);
	evttrace_initialized = 1;
#endif
}

void
event_trace_client_init(evttrace_write_fn_t wrfn)
{
#ifdef EVENT_TRACE_ENABLE
	struct event_trace_info ei;
	unsigned int magic = EVENT_TRACE_START_MAGIC;
	unsigned int cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	unsigned long long st_tsc = 0;
	int i;

	rdtscll(st_tsc);
	memcpy(tracehdr, &magic, sizeof(unsigned int));
	memcpy(tracehdr + sizeof(unsigned int), &cycs, sizeof(unsigned int));
	memcpy(tracehdr + (sizeof(unsigned int) * 2), &st_tsc, sizeof(unsigned long long));
	evttrace_st_tsc = st_tsc;
	evttrace_cpu_cycs_usec = cycs;
	wrfn(tracehdr, EVTTRACE_HDR_SZ);

	evttrace_shmid = channel_shared_page_allocn(EVENT_TRACE_KEY, EVENT_TRACE_NPAGES, (vaddr_t *)&evttrace_vaddr);
	assert(evttrace_shmid && evttrace_vaddr);
	memset((void *)evttrace_vaddr, 0, EVENT_TRACE_NPAGES * PAGE_SIZE);

	evttrace_ring = (volatile struct ck_ring *)EVTTRACE_SHM_RING(evttrace_vaddr);
	ck_ring_init(EVTTRACE_RING, EVTTRACE_RING_SIZE);
	/* 1 page pretty much wasted! */
	evttrace_buf = (volatile struct event_trace_info *)EVTTRACE_SHM_RINGBUF(evttrace_vaddr);

	evttrace_write_fn = wrfn;
	/* test max enq in ck rings */
	for (i = 0; i < EVTTRACE_RING_SIZE-1; i++) {
		int ret = ck_ring_enqueue_spsc_evttrace(EVTTRACE_RING, EVTTRACE_BUF, &ei);

		assert(ret == true);
	}

	i = ck_ring_enqueue_spsc_evttrace(EVTTRACE_RING, EVTTRACE_BUF, &ei);
	assert(i == false);

	PRINTC("Client init done! [capacity: %u][size:%u]\n", ck_ring_capacity(EVTTRACE_RING), ck_ring_size(EVTTRACE_RING));
	ps_cas((unsigned long *)EVTTRACE_SHM_ID(evttrace_vaddr), 0, evttrace_shmid);
	evttrace_initialized = 1;
#endif
}

static int
event_batch_process(int *processed)
{
	static int first = 1;
	unsigned int count = 0;

	if (unlikely(evttrace_initialized == 0)) return 0;
	if (unlikely(ps_load((unsigned long *)EVTTRACE_SHM_SERVER(evttrace_vaddr)) == 0)) return 0;

#ifdef EVTTRACE_BATCH_OUTPUT
	unsigned char flush_buf[EVTTRACE_BATCHBUF_SIZE] = { 0 };
	memset(flush_buf, 0, EVTTRACE_BATCHBUF_SIZE);

	/* mpsc because multiple cfe threads can write. only one rk thread will read */
	while (ck_ring_dequeue_mpsc_evttrace(EVTTRACE_RING, EVTTRACE_BUF,
	       (struct event_trace_info *)(flush_buf + (count * sizeof(struct event_trace_info)))) == true) {
		count++;

		if (unlikely(count == EVTTRACE_BATCH_SIZE)) break;
	}

#ifdef EVTTRACE_DEBUG_TRACE
	if (likely(count)) event_decode(flush_buf, count * sizeof(struct event_trace_info));
#else
	if (likely(count)) evttrace_write_fn(flush_buf, count * sizeof(struct event_trace_info));
#endif

#else
	struct event_trace_info eti;

	memset(&eti, 0, sizeof(struct event_trace_info));
	/* mpsc because multiple cfe threads can write. only one rk thread will read */
	while (ck_ring_dequeue_spsc_evttrace(EVTTRACE_RING, EVTTRACE_BUF, &eti) == true) {

#ifdef EVTTRACE_DEBUG_TRACE
		event_decode(&eti, sizeof(struct event_trace_info));
#else
		evttrace_write_fn(&eti, sizeof(struct event_trace_info));
#endif
		count++;
		memset(&eti, 0, sizeof(struct event_trace_info));
	}

#endif

	*processed = count;
	logged += count;
	if (unlikely(logged && logged % 100000 == 0)) printc("[*%d]", logged);

	return ck_ring_size(EVTTRACE_RING);
}

int
event_flush(void)
{
	int processed = 0, total_processed = 0;

	if (unlikely(evttrace_initialized == 0)) return 0;
	while (event_batch_process(&processed)) total_processed += processed;

	return total_processed;
}

int
event_trace(struct event_trace_info *ei)
{
	int count = 0, ret = 0;
	static int last_skipped = 0;

	/* don't log yet or don't log for components that don't initialize, ex: sl events only for cFE..*/
	if (unlikely(evttrace_initialized == 0)) return 0;

	EVTTRACE_WLOCK_TAKE();
retry:
	/* mpsc because multiple cfe threads can write. only one rk thread will read */
	if (ck_ring_enqueue_spsc_evttrace(EVTTRACE_RING, EVTTRACE_BUF, ei) != true) {
		count++;
		/* TODO: perhaps spit out number of skipped msgs or write directly to serial or something?? */
		if (unlikely(count >= EVTTRACE_RETRY_MAX)) {
			skipped++;
			ret = -1;
			goto done;
		}

		goto retry;
	}

	queued++;

done:
	EVTTRACE_WLOCK_GIVE();
	if (unlikely(ret == -1)) printc("?");
	else if (unlikely(queued % 100000 == 0)) printc("%s", (skipped - last_skipped) ? "!" : "#");
	last_skipped = skipped;

	return ret;
}

#endif

void
event_decode(void *trace, int sz)
{
#ifdef EVTTRACE_DEBUG_TRACE

	struct event_trace_info *evt = NULL;
	int curr = 0, eisz = sizeof(struct event_trace_info);

	assert(evttrace_initialized);
	assert(sz >= eisz);
//	assert(evttrace_cpu_cycs_usec > 0);

	do {
		char trace_msg[EVTTRACE_MSG_LEN] = { 0 };
		unsigned long long tsc = 0;

		evt = (struct event_trace_info *)(trace + curr);
		tsc = evt->ts;
		//tsc -= evttrace_st_tsc;
		sprintf(trace_msg, "[%LF] thread=%u, ", ((long double)tsc/(long double)evttrace_cpu_cycs_usec), evt->thdid);

		switch(evt->type) {
			case SYSCALL_EVENT:
				{
					switch(evt->sub_type) {
						case SYSCALL_THD_SWITCH_START:
							strncat(trace_msg, syscall_msg[evt->sub_type], strlen(syscall_msg[evt->sub_type]));
							//PRINTC(trace_msg, evt->objid);
							PRINTC("%s", trace_msg);
							break;
						case SYSCALL_THD_SWITCH_END:
						case SYSCALL_RCV_START:
						case SYSCALL_RCV_END:
						case SYSCALL_SCHED_RCV_START:
						case SYSCALL_SCHED_RCV_END:
						case SYSCALL_ASND_START:
						case SYSCALL_ASND_END:
						case SYSCALL_SCHED_ASND_START:
						case SYSCALL_SCHED_ASND_END:
							strncat(trace_msg, syscall_msg[evt->sub_type], strlen(syscall_msg[evt->sub_type]));
							PRINTC("%s", trace_msg);
							break;
						default: assert(0);
					}
					break;
				}
			case SL_EVENT:
				{
					switch(evt->sub_type) {
						case SL_BLOCK_START:
						case SL_BLOCK_TIMEOUT_START:
						case SL_YIELD_START:
						case SL_WAKEUP_START:
							strncat(trace_msg, sl_msg[evt->sub_type], strlen(sl_msg[evt->sub_type]));
							//PRINTC(trace_msg, evt->objid);
							PRINTC("%s", trace_msg);
							break;
						case SL_BLOCK_END:
						case SL_BLOCK_TIMEOUT_END:
						case SL_YIELD_END:
						case SL_WAKEUP_END:
							strncat(trace_msg, sl_msg[evt->sub_type], strlen(sl_msg[evt->sub_type]));
							PRINTC("%s", trace_msg);
							break;
						default: assert(0);
					}

					break;
				}
			case QUEUE_EVENT:
				{
					switch(evt->sub_type) {
						case QUEUE_ENQUEUE_START:
						case QUEUE_ENQUEUE_END:
						case QUEUE_DEQUEUE_START:
						case QUEUE_DEQUEUE_END:
							strncat(trace_msg, queue_msg[evt->sub_type], strlen(queue_msg[evt->sub_type]));
							//PRINTC(trace_msg, evt->objid);
							PRINTC("%s", trace_msg);
							break;
						default: assert(0);
					}

					break;
				}
			case MUTEX_EVENT:
				{
					switch(evt->sub_type) {
						case MUTEX_TAKE_START:
						case MUTEX_TAKE_END:
						case MUTEX_GIVE_START:
						case MUTEX_GIVE_END:
							strncat(trace_msg, mutex_msg[evt->sub_type], strlen(mutex_msg[evt->sub_type]));
							//PRINTC(trace_msg, evt->objid);
							PRINTC("%s", trace_msg);
							break;
						default: assert(0);
					}

					break;
				}
			case BINSEM_EVENT:
				{
					switch(evt->sub_type) {
						case BINSEM_TAKE_START:
						case BINSEM_TAKE_END:
						case BINSEM_GIVE_START:
						case BINSEM_GIVE_END:
						case BINSEM_TIMEDWAIT_START:
						case BINSEM_TIMEDWAIT_END:
							strncat(trace_msg, binsem_msg[evt->sub_type], strlen(binsem_msg[evt->sub_type]));
							//PRINTC(trace_msg, evt->objid);
							PRINTC("%s", trace_msg);
							break;
						default: assert(0);
					}

					break;
				}
			case COUNTSEM_EVENT:
				{
					switch(evt->sub_type) {
						case COUNTSEM_TAKE_START:
						case COUNTSEM_TAKE_END:
						case COUNTSEM_GIVE_START:
						case COUNTSEM_GIVE_END:
						case COUNTSEM_TIMEDWAIT_START:
						case COUNTSEM_TIMEDWAIT_END:
							strncat(trace_msg, countsem_msg[evt->sub_type], strlen(countsem_msg[evt->sub_type]));
							//PRINTC(trace_msg, evt->objid);
							PRINTC("%s", trace_msg);
							break;
						default: assert(0);
					}

					break;
				}
			default: /* TODO: remaining events */
			//	assert(0);
				PRINTC("Not an event\n");
				break;
		}

		curr += eisz;

	} while (curr < sz);
#endif
}

#else
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

unsigned long long evttrace_st_tsc = 0;
unsigned int evttrace_cpu_cycs_usec = 0;

void
event_decode(void *trace, int sz)
{
	struct event_trace_info *evt = NULL;
	int curr = 0, eisz = sizeof(struct event_trace_info);

	assert(sz >= eisz);

	do {
		char trace_msg[EVTTRACE_MSG_LEN] = { 0 };
		unsigned long long tsc = 0;

		evt = (struct event_trace_info *)(trace + curr);
		memset(trace_msg, 0, EVTTRACE_MSG_LEN);

		tsc = evt->ts;
		//tsc -= evttrace_st_tsc;
		sprintf(trace_msg, "[%llu] thread=%u, ", tsc, evt->thdid);
		//sprintf(trace_msg, "[%.2LF] thread=%u, ", ((long double)tsc/(long double)evttrace_cpu_cycs_usec), evt->thdid);

		switch(evt->type) {
			case SYSCALL_EVENT:
				{
					switch(evt->sub_type) {
						case SYSCALL_THD_SWITCH_START:
							strncat(trace_msg, syscall_msg[evt->sub_type], strlen(syscall_msg[evt->sub_type]));
							fprintf(stdout, trace_msg, evt->objid);
							break;
						case SYSCALL_THD_SWITCH_END:
						case SYSCALL_RCV_START:
						case SYSCALL_RCV_END:
						case SYSCALL_SCHED_RCV_START:
						case SYSCALL_SCHED_RCV_END:
						case SYSCALL_ASND_START:
						case SYSCALL_ASND_END:
						case SYSCALL_SCHED_ASND_START:
						case SYSCALL_SCHED_ASND_END:
							strncat(trace_msg, syscall_msg[evt->sub_type], strlen(syscall_msg[evt->sub_type]));
							fprintf(stdout, "%s", trace_msg);
							break;
						default:
							fprintf(stdout, "Not an event\n");
							break;
					}
					break;
				}
			case SL_EVENT:
				{
					switch(evt->sub_type) {
						case SL_BLOCK_START:
						case SL_BLOCK_TIMEOUT_START:
						case SL_YIELD_START:
						case SL_WAKEUP_START:
							strncat(trace_msg, sl_msg[evt->sub_type], strlen(sl_msg[evt->sub_type]));
							fprintf(stdout, trace_msg, evt->objid);
							break;
						case SL_BLOCK_END:
						case SL_BLOCK_TIMEOUT_END:
						case SL_YIELD_END:
						case SL_WAKEUP_END:
							strncat(trace_msg, sl_msg[evt->sub_type], strlen(sl_msg[evt->sub_type]));
							fprintf(stdout, "%s", trace_msg);
							break;
						default:
							fprintf(stdout, "Not an event\n");
							break;
					}

					break;
				}
			case QUEUE_EVENT:
				{
					switch(evt->sub_type) {
						case QUEUE_ENQUEUE_START:
						case QUEUE_ENQUEUE_END:
						case QUEUE_DEQUEUE_START:
						case QUEUE_DEQUEUE_END:
							strncat(trace_msg, queue_msg[evt->sub_type], strlen(queue_msg[evt->sub_type]));
							fprintf(stdout, trace_msg, evt->objid);
							break;
						default:
							fprintf(stdout, "Not an event\n");
							break;
					}

					break;
				}
			case MUTEX_EVENT:
				{
					switch(evt->sub_type) {
						case MUTEX_TAKE_START:
						case MUTEX_TAKE_END:
						case MUTEX_GIVE_START:
						case MUTEX_GIVE_END:
							strncat(trace_msg, mutex_msg[evt->sub_type], strlen(mutex_msg[evt->sub_type]));
							fprintf(stdout, trace_msg, evt->objid);
							break;
						default:
							fprintf(stdout, "Not an event\n");
							break;
					}

					break;
				}
			case BINSEM_EVENT:
				{
					switch(evt->sub_type) {
						case BINSEM_TAKE_START:
						case BINSEM_TAKE_END:
						case BINSEM_GIVE_START:
						case BINSEM_GIVE_END:
						case BINSEM_TIMEDWAIT_START:
						case BINSEM_TIMEDWAIT_END:
							strncat(trace_msg, binsem_msg[evt->sub_type], strlen(binsem_msg[evt->sub_type]));
							fprintf(stdout, trace_msg, evt->objid);
							break;
						default:
							fprintf(stdout, "Not an event\n");
							break;
					}

					break;
				}
			case COUNTSEM_EVENT:
				{
					switch(evt->sub_type) {
						case COUNTSEM_TAKE_START:
						case COUNTSEM_TAKE_END:
						case COUNTSEM_GIVE_START:
						case COUNTSEM_GIVE_END:
						case COUNTSEM_TIMEDWAIT_START:
						case COUNTSEM_TIMEDWAIT_END:
							strncat(trace_msg, countsem_msg[evt->sub_type], strlen(countsem_msg[evt->sub_type]));
							fprintf(stdout, trace_msg, evt->objid);
							break;
						default:
							fprintf(stdout, "Not an event\n");
							break;
					}

					break;
				}
			default: /* TODO: remaining events */
			//	assert(0);
				fprintf(stdout, "Not an event\n");
				break;
		}

		curr += eisz;

	} while (curr < sz);
}

static unsigned int
convert_hex_to_bin(char *trace, char *trace_bin)
{
	char *tok = strtok(trace, " ");
	unsigned int cur = 0;

	do {
		int val = 0;

		assert(strlen(tok) == 2);
		if (tok[0] >= '0' && tok[0] <= '9') val = (tok[0] - '0') * 16;
		else val = (tok[0] - 'a' + 10) * 16;
		if (tok[1] >= '0' && tok[1] <= '9') val += (tok[1] - '0');
		else val += (tok[1] - 'a' + 10);

		((unsigned char *)trace_bin)[cur] = (unsigned char)val;
		cur++;
	} while ((tok = strtok(NULL, " ")) != NULL);


#ifdef EVTTRACE_DEBUG_TRACE
	printf("******************************\n");
	printf("%s\n", trace);
	printf("******************************\n");
	int i = 0;
	for (; i < cur; i++) {
		printf("%.2hx ", ((unsigned char *)trace_bin)[i]);
	}
	printf("\n");
#endif

	return cur;
}

void *
event_trace_check_hdr(void *trace_bin, unsigned int *sz)
{
	unsigned int start_off = 0;

	while (*((unsigned int *)(((unsigned char *)trace_bin) + start_off)) != EVENT_TRACE_START_MAGIC) {
		start_off++;
	}

	if (start_off + sizeof(unsigned int) >= *sz) assert(0);
	start_off += sizeof(unsigned int);

	evttrace_cpu_cycs_usec = *((unsigned int *)((unsigned char *)trace_bin + start_off));
	start_off += sizeof(unsigned int);

	assert(evttrace_cpu_cycs_usec);

	evttrace_st_tsc = *((unsigned long long *)((unsigned char *)trace_bin + start_off));
	start_off += sizeof(unsigned long long);
	printf("Start time (event trace init): %llu\nCPU cycles per usecs: %u\n", evttrace_st_tsc, evttrace_cpu_cycs_usec);
	printf("******************************\n");

	*sz -= start_off;

	return (void *)((unsigned char *)trace_bin + start_off);
}

#ifdef EVENT_TRACE_REMOTE
int
main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: %s <cos_cfs_ipaddr>\n", argv[0]);

		return -1;
	}
	udp_trace_loop(argv[1], EVENT_TRACE_INPORT, EVENT_TRACE_OUTPORT);

	return 0;
}
#else

int
main(int argc, char **argv)
{
	int fd = -1;
	void *trace = NULL, *trace_bin = NULL;
	struct stat tsb;
	unsigned int trace_sz = 0;
	char *skip = NULL;

	if (argc != 2) {
		printf("Usage: %s <input_file_path>\n", argv[0]);
		goto abort;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		goto abort;
	}

	if (fstat(fd, &tsb) > 0) {
		perror("fstat");
		goto error;
	}

	trace = malloc(tsb.st_size);
	trace_bin = malloc(tsb.st_size);
	if (!trace || !trace_bin) {
		printf("malloc failed\n");
		goto error;
	}

	memset(trace, 0, tsb.st_size);
	memset(trace_bin, 0, tsb.st_size);
	if (read(fd, trace, tsb.st_size) != tsb.st_size) {
		perror("read");
		goto error;
	}
	close(fd);
	trace_sz = convert_hex_to_bin(trace, trace_bin);
	printf("Original size: %u\nTrace size: %u\n", tsb.st_size, trace_sz);
	printf("******************************\n");
	event_decode(event_trace_check_hdr(trace_bin, &trace_sz), trace_sz);
	printf("******************************\n");

	return 0;

error:
	close(fd);
abort:
	exit(1);
}
#endif

#endif
