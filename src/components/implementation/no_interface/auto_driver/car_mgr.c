#include <cos_kernel_api.h>
#include <sched.h>

void
car_main(void)
{
	printc("car main, blocking.\n");
	sched_thd_block(0);
	assert(0);
}

void cos_init(void)
{
	printc("\n--------------- Welcome to the car_mgr component -------\n");

	car_main();	
}
