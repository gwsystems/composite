#include <cos_component.h>

extern int print_vals(int a, int b, int c);

void foo(int print, unsigned int val)
{
	if (print) print_vals(val, 0, 0);

	return;
}
