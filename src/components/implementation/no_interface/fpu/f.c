#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <timed_blk.h>

void cos_init(void *args)
{
	double a = 0.5, b = 0.5, c = 0;

	timed_event_block(cos_spd_id(), 1);

	while(1)
	{
		c += a + b;

		if((int)c < 0) {
			c = 0;
			printc("wanle\n");
			//break;
		}
		if(((int)c > 0) && ((int)c % 10000 == 0))
			printc("calc by thread %d, counter = %d\n", cos_get_thd_id(), ((int)c/10000));
	}
}

