#include <cos_component.h>
#include <print.h>
#include <sched.h>


void cos_init(void *args)
{
	double f = 1;
	for(int i = 0; i<10; i++)
	{
		f *= 0.1;
		printc("%lf\n", f);
	}
}

