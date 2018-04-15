#include <cos_kernel_api.h>
#include <cos_component.h>
#include <sched.h>
#include <robot_cont.h>

#include <memmgr.h>

struct cos_aep_info taeps[2];

void
car_main(void)
{
	send_task(3,2);
	sched_thd_block(0);
}

void
driver_aep(arcvcap_t rcv, void * data)
{
	int ret;

	printc("car_aep_test\n");
	while(1) {
		ret = cos_rcv(rcv, 0, NULL);
		assert(ret == 0);
		printc("driver_aep post cos_rcv\n");
		send_task(3,2);

	}
}

#define DRIVER_AEPKEY 1
#define DRIVER_PRIO 1

void 
cos_init(void)
{
	printc("\n--------------- Welcome to the car_mgr component -------\n");

	thdid_t tidp;
	asndcap_t parentasnd;
	int i = 0;

	printc("Creating aep for driver\n");
	tidp = sched_aep_create(&taeps[1], driver_aep, (void *)i, 0, DRIVER_AEPKEY);
	assert(tidp);
	printc("Created aep for driver\n");
	sched_thd_param_set(tidp, sched_param_pack(SCHEDP_PRIO, DRIVER_PRIO));

	car_main();	
	
	assert(0);
}
