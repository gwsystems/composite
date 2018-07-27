#include <event_trace.h>

#undef EVTTRACE_DEBUG_TRACE
static int evttrace_initialized = 0;

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

#ifndef LINUX_DECODE

#include <ck_ring.h>
#include <cos_serial.h>
#include <ps_plat.h>

#define EVTTRACE_RING_SIZE 1024
/*
 * FIXME:
 * For now, tracing only in this component.
 */
static struct ck_ring evttrace_ring;
static struct event_trace_info evttrace_buf[EVTTRACE_RING_SIZE];
/* for now, using it for only serial writes */
static struct ps_lock evttrace_lock;
unsigned long long evttrace_st_tsc = 0;
unsigned int evttrace_cpu_cycs_usec = 0;

/* TODO: no locks yet. could interleave here! */
void
event_trace_init(void)
{
	unsigned char tracehdr[EVTTRACE_HDR_SZ] = { 0 };
	unsigned int magic = EVENT_TRACE_START_MAGIC;
	unsigned int cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	unsigned long long st_tsc = 0;

	PRINTC("Event trace initialization!\n");
	ps_lock_init(&evttrace_lock);
	memset(evttrace_buf, 0, sizeof(struct event_trace_info) * EVTTRACE_RING_SIZE);
	ck_ring_init(&evttrace_ring, EVTTRACE_RING_SIZE);
	rdtscll(st_tsc);
	memcpy(tracehdr, &magic, sizeof(unsigned int));
	memcpy(tracehdr + sizeof(unsigned int), &cycs, sizeof(unsigned int));
	memcpy(tracehdr + (sizeof(unsigned int) * 2), &st_tsc, sizeof(unsigned long long));
	evttrace_st_tsc = st_tsc;
	evttrace_cpu_cycs_usec = cycs;

	serial_print((void *)&tracehdr, EVTTRACE_HDR_SZ);

	evttrace_initialized = 1;
}

static int
event_flush(void)
{
	unsigned char flush_buf[LLPRINT_SERIAL_MAX_LEN] = { 0 };
	struct event_trace_info evtinfo;
	int count = 0, max = LLPRINT_SERIAL_MAX_LEN / sizeof(struct event_trace_info), ret_count = 0;

	assert(evttrace_initialized);

	memset(flush_buf, 0, LLPRINT_SERIAL_MAX_LEN);
	while (ck_ring_dequeue_mpmc_evttrace(&evttrace_ring, evttrace_buf, &evtinfo) == true) {


		/* batch writing to serial */
		memcpy(flush_buf + (sizeof(struct event_trace_info) * count), &evtinfo, sizeof(struct event_trace_info));
		count++;
		assert(count <= max);

		if (count == max) {
			ret_count += count;
			count = 0;
#ifndef EVTTRACE_DEBUG_TRACE
			serial_print(flush_buf, sizeof(struct event_trace_info) * max);
#else
			event_decode((void *)flush_buf, sizeof(struct event_trace_info) * max);
#endif
			memset(flush_buf, 0, LLPRINT_SERIAL_MAX_LEN);
		}
	}

	ret_count += count;
	if (count) {
#ifndef EVTTRACE_DEBUG_TRACE
		serial_print(flush_buf, sizeof(struct event_trace_info) * count);
#else
		event_decode((void *)flush_buf, sizeof(struct event_trace_info) * count);
#endif
	}

	return ret_count;
}

int
event_trace(struct event_trace_info *ei)
{
	/* don't log yet or don't log for components that don't initialize, ex: sl events only for cFE..*/
	if (unlikely(evttrace_initialized == 0)) return 0;

retry:
	/* mpmc because any thread could enqueue events and dequeue(flush out to serial) */
	if (ck_ring_enqueue_mpmc_evttrace(&evttrace_ring, evttrace_buf, ei) != true) {
		event_flush();

		goto retry;
	}

	return 0;
}

void
event_decode(void *trace, int sz)
{
#ifdef EVTTRACE_DEBUG_TRACE

	struct event_trace_info *evt = NULL;
	int curr = 0, eisz = sizeof(struct event_trace_info);

	assert(evttrace_initialized);
	assert(sz >= eisz);
	assert(evttrace_cpu_cycs_usec > 0);

	do {
		char trace_msg[EVTTRACE_MSG_LEN] = { 0 };
		unsigned long long tsc = 0;

		evt = (struct event_trace_info *)(trace + curr);
		memset(trace_msg, 0, EVTTRACE_MSG_LEN);
		tsc = evt->ts;
		tsc -= evttrace_st_tsc;
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
event_trace_init(void)
{ assert(0); }

int
event_trace(struct event_trace_info *ei)
{ assert(0); }

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
							fprintf(stdout, trace_msg, evt->objid);
							break;
						case SL_BLOCK_END:
						case SL_BLOCK_TIMEOUT_END:
						case SL_YIELD_END:
						case SL_WAKEUP_END:
							strncat(trace_msg, sl_msg[evt->sub_type], strlen(sl_msg[evt->sub_type]));
							fprintf(stdout, "%s", trace_msg);
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
							fprintf(stdout, trace_msg, evt->objid);
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
							fprintf(stdout, trace_msg, evt->objid);
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
							fprintf(stdout, trace_msg, evt->objid);
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
							fprintf(stdout, trace_msg, evt->objid);
							break;
						default: assert(0);
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

static void *
trace_check_hdr(char *trace_bin, unsigned int *sz)
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
	printf("******************************\n");
	event_decode(trace_check_hdr(trace_bin, &trace_sz), trace_sz);
	printf("******************************\n");

	return 0;

error:
	close(fd);
abort:
	exit(1);
}

#endif
