#include <cos_component.h>

extern int sched_init(void);

int nothing = 0;

#define ITER 100000

int spd0_main(void)
{
	int ret;

	//for (i = 0 ; i < ITER ; i++) {
	ret = sched_init();
	//}

	nothing = ret + 1;
	//prevent_tail_call(ret);
	return ret;
}

void bag(void)
{
	sched_init();
}
