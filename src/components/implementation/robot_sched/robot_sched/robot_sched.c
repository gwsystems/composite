#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <robot_cont.h>
#include <cos_alloc.h>

int
assign_task(unsigned long token, int x, int y) {
	printc("In assign_task() in robot sched interface\n");

	return 1;
}


void
cos_init(void)
{
	printc("Welcome to the robot sched component\n");

	send_task(2, 3);
	cos_sinv(BOOT_CAPTBL_SINV_CAP, INIT_DONE, 2, 3, 4);
	return;
}


