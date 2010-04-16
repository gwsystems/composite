#include <cos_component.h>
#include <print.h>
#include <sched.h>

void cos_init(void *arg)
{
//	while (1) ;
	printc("hello world %d!\n", (int)cos_spd_id());
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
