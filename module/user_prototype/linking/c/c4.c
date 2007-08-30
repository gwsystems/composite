#include <cos_component.h>

extern int spd3_main(void);
int spd4_main(int v)
{
//	return spd3_main();
	return spd3_main() + v;
}

int spd4_noarg(void)
{
	int ret = spd3_main();
	prevent_tail_call(ret);
	return ret;
}
