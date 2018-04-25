#include <cos_kernel_api.h>
#include <cos_component.h>
#include <sched.h>
#include <robot_cont.h>
#include <gateway_spec.h>
#include <memmgr.h>

struct cos_aep_info taeps[2];

void
car_main(void)
{
	sched_thd_block(0);
}

void
driver_aep(arcvcap_t rcv, void * data)
{
	int ret;
	static int demo = 0;

	printc("car_aep_test\n");
	while(1) {
		ret = cos_rcv(rcv, 0, NULL);
		assert(ret == 0);

		if (!demo) {
			demo = 1;
		} else {
			printc("Demo done (but can continue if needed)\n");
			continue;
		}

		printc("Driver 1 sending task\n");
		send_task(5, 7, 2);
	}
}

#define DRIVER_PRIO 1

void 
cos_init(void)
{
	printc("\n--------------- Welcome to the car_mgr component -------\n");

	thdid_t tidp;
	int i = 0;

	printc("Creating aep for driver\n");
	tidp = sched_aep_create(&taeps[1], driver_aep, (void *)i, 0, DRIVER_AEP_KEY);
	assert(tidp);
	printc("Created aep for driver\n");
	sched_thd_param_set(tidp, sched_param_pack(SCHEDP_PRIO, DRIVER_PRIO));

	car_main();	
	
	assert(0);
}
