#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <micro_pong.h>

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
