#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <timed_blk.h>

void cos_init(void *args)
{
	double f = 1.5, g = 2.5, h = 0;
	int i;

	while(1){
	for (i = 0 ; i < 100000 ; i++) {
		h += f + g;
	}
	
		printc("calc by thread %d", cos_get_thd_id());
		printc(" | h = %d\n", (int)h);
		timed_event_block(cos_spd_id(), 5);
}

}

