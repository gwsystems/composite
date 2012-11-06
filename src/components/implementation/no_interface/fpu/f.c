#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <timed_blk.h>

void cos_init(void *args)
{
	double f = 0.5, g = 0.5, h = 0;

	timed_event_block(cos_spd_id(), 3);

	while(1)
	{
		
		h += f + g;
		//if((int)h < 0) // avoid the value exceeds the limit of int, make it a infinite loop 
		//	h = 1;
		//if((int)h % 100 == 0)
		//printc("calc by thread %d, h = %d\n", cos_get_thd_id(), (int)h);
		printc(".%d-%d.", cos_get_thd_id(), (int)h);
	}
}

