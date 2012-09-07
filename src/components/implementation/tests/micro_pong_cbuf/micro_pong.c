#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <micro_pong.h>

//#define VERBOSE
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif


void call(void)
{
	return;
}

int call_buf2buf(u32_t cb, int len) 
{
	u64_t start = 0, end = 0;
	char *b;
	
	rdtscll(start);
	b = cbuf2buf(cb,len);
	rdtscll(end);

        printc("cbuf2buf %llu cycs\n", end-start);
	
	if (!b) {
		printc("Can not map into this spd %ld\n", cos_spd_id());
		return cbuf_null();
	}
	memset(b, 's', len);
	
	return 0;
}

int simple_call_buf2buf(u32_t cb, int len) 
{
	char *b;
	b = cbuf2buf(cb,len);
	return 0;
}

void call_cs(void)
{
	static int first = 0;
	static int high, low;
	u64_t start = 0, end = 0;

	if(first == 1){
		low = cos_get_thd_id();
		sched_wakeup(cos_spd_id(), high);
	}

	if(first == 0){
		first = 1;
		high = cos_get_thd_id();
		sched_block(cos_spd_id(), 0);
		rdtscll(start);
		sched_block(cos_spd_id(), low);
	}

	if (cos_get_thd_id() == low) {
		sched_wakeup(cos_spd_id(), high);
	}

	if (cos_get_thd_id() == high) {
		rdtscll(end);
		printc("context switch cost: %llu cycs\n", (end-start) >> 1);
		first = 0;
	}
	return;
}


