#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <timed_blk.h>
#include <hlc.h>
 
#define ITER 10000


static int create_thd(const char *pri)
{
	struct cos_array *data;
	int event_thd;
	int sz = strlen(pri) + 1;
    
	data = cos_argreg_alloc(sizeof(struct cos_array) + sz);
	assert(data);
	strcpy(&data->mem[0], pri);
	//data->sz = 4;
	data->sz = sz;

	if (0 > (event_thd = sched_create_thread(cos_spd_id(), data))) assert(0);
	cos_argreg_free(data);
    
	return event_thd;
}

void cos_init(void)
{
    
	u64_t start, end;
	int i, j;
   	static int first = 0;
	static int hthd;
	static int lthd;
	if(first == 0){
		hthd = create_thd("r-1");
		lthd = cos_get_thd_id();
		for(j = 0; j < 3; j++){
			create_thd("r1");
		}
		first = 1;
	}

	if(cos_get_thd_id() == hthd){
		u64_t tot = 0;
		timed_event_block(cos_spd_id(), 9);

		for (i = 0 ; i < ITER ; i++) {
			rdtscll(start);
			call_high();
			rdtscll(end);
			tot += end-start;
			timed_event_block(cos_spd_id(), 1);
		}
		printc("%d invocations, avg %lld\n", ITER, tot/(u64_t)ITER);
	}
	else if(cos_get_thd_id() == lthd){
		timed_event_block(cos_spd_id(), 9);
		for (i = 0 ; i < ITER ; i++) {
			call_low();
		}
	} else {
		printc("Starting interference thread %d\n", cos_get_thd_id());
		call_low();
	}

	return;
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
