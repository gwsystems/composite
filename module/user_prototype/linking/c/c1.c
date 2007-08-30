#include <cos_component.h>

extern int spd4_main(int v);
int spd1_call4(void)
{
	int ret = spd4_main(4);
	prevent_tail_call(ret);
	return ret;
}

int spd1_main(void)
{
	return 1;
}

