#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <timed_blk.h>

void cos_init(void *args)
{
	double a = 0.5, b = 0.5, c = 0;
	int i = 0;

	timed_event_block(cos_spd_id(), 3);

	while(1)
	{
		c += a + b;
		i++;

		if((int)c > 0) {
			if(i == 100000) {
			i = 0;
				printc("calc by thread %d, counter = %d\n", cos_get_thd_id(), ((int)c));
			}
		}
	}
}

