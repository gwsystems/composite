#include <cos_component.h>

#define ITER 10
extern void foo(int print, unsigned int val);

void bar(unsigned int val)
{
	int i;

	for (i = 0 ; i < ITER ; i++) {
		foo(0, val);
	}

	foo(1, val);

	return;
}
