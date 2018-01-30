#include <cos_kernel_api.h>
#include <robot_sched.h>
#include <cos_types.h>
#include <cobj_format.h>

void cos_init(void)
{
	printc("\nWelcome to the car_mgr component\n");

	
	assign_task(4,1,1);
	assign_task(4,0,0);
	assign_task(4,0,1);
//	assign_task(4,1,0);

	printc("car_mgr1 tasks done\n");
	while(1);
	cos_sinv(BOOT_CAPTBL_SINV_CAP, INIT_DONE, 2, 3, 4);
	return;
}
