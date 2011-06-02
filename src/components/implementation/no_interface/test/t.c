#include <cos_component.h>
#include <print.h>
#include <sched.h>

volatile int blah = 0;

void cos_init(void *arg)
{
	int other = *((int*)0);//, other2 = 10/blah;

	printc("hello world %d!\n", (int)cos_spd_id());
	asm volatile ("int $0xe9");
	printc("hello world %d!\n", (int)cos_spd_id());
	other = 10;
}

void bin(void)
{
	sched_block(cos_spd_id(), 0);
}
