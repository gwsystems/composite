#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <timed_blk.h>

/*
double thread_1()
{
	double f = 1.5, g = 2.5, h = 0;
	int i;
	for(i = 0; i < 1000; i++){
		timed_event_block(cos_spd_id(), 100);
		h += (f + g) * cos_get_thd_id();
		printc("calc by thread %d", cos_get_thd_id());
		printc(" | h1 = %d\n", (int)h);
	}
	return h;
}


double thread_2()
{
	double f = 1.5, g = 1.5, h = 0;
	int i;
	for(i = 0; i < 1000; i++){
		timed_event_block(cos_spd_id(), 110);
		h += (f + g) * cos_get_thd_id();
		printc("calc by thread %d", cos_get_thd_id());
		printc(" | h2 = %d\n", (int)h);
	}
	return h;
}
*/
void cos_init(void *args)
{
	//double h1 = thread_1();
	//double h2 = thread_2();
	double f = 1.5, g = 2.5, h = 0;
	int i;
	for(i = 0; i < 1000; i++){
		timed_event_block(cos_spd_id(), 100);
		h += (f + g) * cos_get_thd_id();
		printc("calc by thread %d", cos_get_thd_id());
		printc(" | h = %d\n", (int)h);
	}
	//double result = h1 + h2;
}

