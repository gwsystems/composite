#include <cos_component.h>

#define ITER 500000
extern void foo(int print, unsigned int val, unsigned int val2);
extern int print_vals(int a, int b, int c);

void bar(unsigned int val, unsigned int val2)
{
	int i;

	//cos_mpd_cntl(COS_MPD_DEMO);

//	print_vals(7337, 0, 3);
//	print_vals(7337, 1, 3);
	for (i = 0 ; i < ITER ; i++) {
		foo(0, val, val2);
	}
	//cos_mpd_cntl(COS_MPD_DEMO);

//	foo(1, val, val2);
//	print_vals(7337, 2, 3);

	return;
}

void symb_bin(void)
{
	print_vals(0, 0, 0);
}
