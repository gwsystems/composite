#include <cos_component.h>

extern int spd1_main(void);
extern int spd1_call4(void);
extern int spd4_main(int v);
//extern int spd2_main(void);
//extern int spd2_other(void);
//extern int spd3_main(void);
#define ITER 10000

int t(int v)
{
	return v+1;
}

int other(void) __attribute__((noinline));
int other(void)
{
	int i;
	for (i = 0 ; i < ITER ; i++) {
		spd1_call4();
		//spd1_main();
	}
	
//	return spd1_main();
//	spd4_main(1);

	i = spd1_main();
	//i = spd1_call4();
	prevent_tail_call(i);
	return i;
}

int spd0_main(void)
{
	int ret = other();
	prevent_tail_call(ret);
	return ret;
}
