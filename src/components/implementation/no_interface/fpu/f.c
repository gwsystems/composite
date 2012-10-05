#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <timed_blk.h>

void cos_init(void *args)
{
	double f = 1.5, g = 2.5, h;
	int i;
	timed_event_block(cos_spd_id(), 3);

	h = f + g;
	printc("test\n");
	printc("%d\n", (int)h);
	//printc("%lf\n", 0.0001);
	for(i = 0; i<10; i++)
	{
		f *= 0.1;
		//printc("%lf\n", f);
	}
	while (1) {
		for (i = 0 ; i < 10000000 ; i++) ;
		printc("%d", cos_get_thd_id());
	}
}

