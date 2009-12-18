#include <cos_component.h>

extern int sched_root_init(void);

int nothing = 0;

int spd0_main(void)
{
	int ret;

	ret = sched_root_init();

	nothing = ret + 1;
	//prevent_tail_call(ret);
	return ret;
}
