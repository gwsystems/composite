#include <cos_component.h>

extern int print_vals(int a, int b, int c);

void foo(int print, unsigned int val, unsigned int val2)
{
	if (print) {
		print_vals(val, val2, 0);
//		print_vals(7337, 0, 4);
//		cos_mpd_cntl(COS_MPD_DEMO);
//		print_vals(7337, 1, 4);
	}

	return;
}
