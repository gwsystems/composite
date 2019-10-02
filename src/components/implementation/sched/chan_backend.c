#include <chan_crt.h>

#include <crt_chan.h>
#include <sched_info.h>

struct __sched_inout_chan {
	struct crt_chan *in, *out;
} __sched_thds[NUM_CPU][MAX_NUM_THREADS];

void
__sched_stdio_init(void)
{
	memset(__sched_thds[cos_cpuid()], 0, MAX_NUM_THREADS * sizeof(struct __sched_inout_chan));
}

void
__sched_stdio_thd_init(thdid_t tid, struct crt_chan *in, struct crt_chan *out)
{
	__sched_thds[cos_cpuid()][tid].in  = in;
	__sched_thds[cos_cpuid()][tid].out = out;
}

int
chan_out(unsigned long item)
{
	struct crt_chan *co = __sched_thds[cos_cpuid()][cos_thdid()].out;

	assert(co != NULL);
	return crt_chan_send_LU(co, &item);
}

unsigned long
chan_in(void)
{
	unsigned long item = 0;
	int ret = 0;
	struct crt_chan *ci = __sched_thds[cos_cpuid()][cos_thdid()].in;

	assert(ci != NULL);

	ret = crt_chan_recv_LU(ci, &item);
	assert(ret == 0);

	return item;	
}
