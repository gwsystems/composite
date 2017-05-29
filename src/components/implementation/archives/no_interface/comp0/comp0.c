#include <cos_component.h>

#include <sched_hier.h>

int nothing = 0, ret = 0;
volatile int mem = 0;

int spd0_main(void)
{
	call_cap(4, 0, 0, 0, 0);

	if (mem) sched_init(); // we need to link the cap!

	return ret;
}
