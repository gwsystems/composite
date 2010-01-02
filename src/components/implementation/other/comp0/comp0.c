#include <cos_component.h>

#include <sched.h>

int nothing = 0, ret = 0;

int spd0_main(void)
{
	sched_root_init();

	nothing = ret + 1;
	//prevent_tail_call(ret);
	return ret;
}
