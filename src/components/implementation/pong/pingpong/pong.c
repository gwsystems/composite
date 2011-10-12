#include <cos_component.h>
#include <print.h>
#include <cbuf.h>
#include <pong_lower.h>
#include <sched.h>

//#define CONTEXT_SWITCH
#define JUST_RETURN
//#define MEASURE_STK_PIP
//#define MEASURE_CBUF_PIP


#ifdef CONTEXT_SWITCH  
void call(void) 
{ 
	static int flag = 0;

	static int first,  second;
	
	u64_t start = 0, end = 0;

	if(flag == 1){
		/* printc("2\n"); */
		second = cos_get_thd_id();
		sched_wakeup(cos_spd_id(), first);
		/* printc("4\n");  */
	}

	if(flag == 0){
		/* printc("1\n"); */
		flag = 1;
		first = cos_get_thd_id();
		sched_block(cos_spd_id(), 0);
		/* printc("3\n"); */
		rdtscll(start);
		sched_block(cos_spd_id(), second);
		/* printc("6\n"); */
	}

	if (cos_get_thd_id() == second)
		/* printc("5\n"); */
		sched_wakeup(cos_spd_id(), first);

	if (cos_get_thd_id() == first) {
		/* printc("7\n"); */
		rdtscll(end);
		printc("cost of basics %llu cycs\n", end-start);
	}

	return; 
}
#endif
 

#ifdef MEASURE_STK_PIP
void call(void) 
{ 
	static int first = 0;
	static int low = 0, high = 0;
	
	if (first == 0 ){
		high = cos_get_thd_id();
		low = high + 1;
		first = 1;
	}

	u64_t start = 0, end = 0;

	int j = 0;
	while(j++ < 10){
		if (cos_get_thd_id() == high) {
			/* printc("p3\n"); */
			sched_block(cos_spd_id(), 0);
			/* printc("p4\n"); */
			rdtscll(start);
		}
		/* printc(" thd %d is calling lower \n", cos_get_thd_id()); */

		call_lower(low, high);
 
		if (cos_get_thd_id() == high) {
			rdtscll(end);
			printc("cost of cached stkPIP %llu cycs\n", end-start);
		}
	}
	return; 
}
#endif

#ifdef MEASURE_CBUF_PIP
void call(void) 
{ 
	static int first = 0;
	static int low = 0, high = 0;
	
	if (first == 0 ){
		high = cos_get_thd_id();
		low = high + 1;
		first = 1;
	}
	int j = 0;
	while(j++ < 100){
		if (cos_get_thd_id() == high) {
			/* printc("put thd %d to sleep\n", high); */
			/* printc("p3\n"); */
			sched_block(cos_spd_id(), 0);
			/* printc("thd %d is up\n", high); */
			/* printc("p4\n"); */
		}

		call_lower(low, high);
	}
	return; 
}
#endif

#ifdef JUST_RETURN
void call(void)
{
	return;
}
#endif

int call_buf2buf(cbuf_t cb, int len, int flag) { 
	u64_t start = 0, end = 0;
	char *b;
	if (flag) 
		rdtscll(start);

	b = cbuf2buf(cb,len);

	if (flag) 
		rdtscll(end);

        if (flag) printc("b2bcost %llu cycs\n", end-start);
	if (!b) {
		printc("Can not map into this spd %ld\n", cos_spd_id());
		return cbuf_null();
	}
	memset(b, 's', len);
	
	return 1;
}

