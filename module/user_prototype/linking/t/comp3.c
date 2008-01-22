#include <cos_component.h>

#define ITER 1500000
extern void foo(int print, unsigned int val, unsigned int val2);

void bar(unsigned int val, unsigned int val2)
{
	int i;

	for (i = 0 ; i < ITER ; i++) {
		foo(0, val, val2);
	}

	foo(1, val, val2);

	return;
}
