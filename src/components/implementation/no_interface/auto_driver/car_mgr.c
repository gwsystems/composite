#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cobj_format.h>

void cos_init(void)
{
	printc("\n--------------- Welcome to the car_mgr component -------\n");
	int itr = 0;

	
//	assign_task(4,1,1);
//	assign_task(4,0,0);
//	assign_task(4,0,1);
//	assign_task(4,1,0);

	while(1) {
		if (itr %100 == 0) {
			printc("car mgr component running\n");
		}
		itr ++;
	}
	return;
}
