#include <cos_component.h>

extern int print_vals(int a, int b, int c);

void foo(int print, unsigned int val, unsigned int val2)
{
	if (print) print_vals(val, val2, 0);

	return;
}
