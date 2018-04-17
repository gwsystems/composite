#include <cos_kernel_api.h>
#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <memmgr.h>
#include <udpserver.h>
#include <sched.h>
#include <capmgr.h>

#include <camera.h>

#include <robot_cont.h>
#include <gateway_spec.h>

#define DRIVER_AEPKEY 1

#define NORTH 0
#define EAST 1
#define SOUTH 2
#define WEST 3

asndcap_t driver_asnd;
cycles_t cycs_per_usec;

struct rp {
	int x, y;
	unsigned long direction;
};
struct rp rpos;

struct cos_aep_info taeps;
struct cos_aep_info taepsdumb;
vaddr_t shmem_addr = NULL;

int
update_script(int x)
{
	return 0;
}

int
create_movement(int xf, int yf) {

	int ychange = yf - rpos.y;
	int xchange = xf - rpos.x;
	int i;
	cycles_t wakeup, now;
	int exp_roomba_location;

	/* roomba scripts can be 100 bytes long */
	/* copy generated script into shared memory for the udpserver, and notify it of an update */

	/* Forward 1 small block */
	char straight [8] = {137, 1, 44, 128, 0, 156, 1, 144};
	/* Forward 1 large block */
	char straight_l [8] = {137, 1, 44, 128, 0, 156, 3, 232};
	/* Clockwise 90 degrees */
	char east [8] = {137, 1, 44, 255, 255, 157, 255, 166};
	/* Counter clockwise 180 degrees */
	char east_180 [8] = {137, 1, 44, 0, 1, 157, 0, 180};
	/* 90 counter clock */
	char east_90 [8] = {137, 1, 44, 0, 1, 157, 0, 90};

	unsigned char script[100] = {152, 29, 137, 0, 200, 0,   1, 157, 0, 90,  // Left turn 90
					      137, 0, 200, 128, 0, 156, 1, 144, // Forward one block
					      137, 1, 44, 0,   1, 157, 0, 90,  // Left turn 90
//					      137, 1, 44, 128, 0, 156, 3, 232, // Forward 1 EW block
//					      137, 1, 44, 128, 0, 156, 1, 144, // Forward 1 NS block
//					      137, 1, 44, 0,   1, 157, 0, 90,  // Left turn 90
//					      137, 1, 44, 128, 0, 156, 1, 144, // Forward one block
					      137, 0, 0,  0,   0, 153, SCRIPT_END};

	/* TODO: Insert logic for determination */
	exp_roomba_location = 3;

#ifdef DEMO1
	printc("Demo1:\n");
	
	/* Verify Script with Camera component */
	udpserv_script(REQ_JPEG, 4);

	int ret = check_location_image(0, 0);
	while (!ret) {
		printc("loading image data\n");
		rdtscll(now);
		wakeup = now + (10000 * 1000 * cycs_per_usec);
		sched_thd_block_timeout(0, wakeup);
		
		ret = check_location_image(0, 0);
	}

	if (ret != 7 && ret != 8) printc("Image Checks out, send script\n");

	printc("shmem: %p \n", (char *) shmem_addr);
	memcpy((unsigned char *)shmem_addr, script, 100);	
	udpserv_script(SEND_SCRIPT, 8);
	
#elif DEMO2
	printc("Demo2:\n");
	printc("shmem: %p \n", (char *) shmem_addr);
	memcpy((unsigned char *)shmem_addr, script, 100);	
	udpserv_script(SEND_SCRIPT, 8);
	
	/* Verify Script with Camera component */
	udpserv_script(REQ_JPEG, 4);
	int ret = check_location_image(0, 0);
	while (!ret) {
		printc("image unavailable\n");
		rdtscll(now);
		wakeup = now + (10000 * 1000 * cycs_per_usec);
		sched_thd_block_timeout(0, wakeup);
		
		ret = check_location_image(0, 0);
	}
	printc("Image Checks out, send script\n");

	if (ret != exp_roomba_location) {
		printc("ROOMBA COMPROMISED, ABORT: %d\n", ret);
		printc("shmem: %p \n", (char *) shmem_addr);
		memcpy((unsigned char *)shmem_addr, script, 100);	
		udpserv_script(SEND_SHUTDOWN, 8);
	}
#endif

	rpos.x = xf;
	rpos.y = yf;	
	
	return 0;
}

int
send_task(int x, int y) 
{
	static int task_in_progress = 0;
	if (!shmem_addr || task_in_progress) return -1;

	printc("\nsend_task\n");
	task_in_progress = 1;	
	create_movement(3,3);
	printc("\n");
	
	//cos_asnd(driver_asnd, DRIVER_AEPKEY);
	
	return 0;
}

void
task_complete_aep(arcvcap_t rcv, void * data)
{
	int ret;
	static int first = 0;
	printc("task_complete_aep\n");
	while(1) {
		ret = cos_rcv(rcv, 0, NULL);
		assert(ret == 0);
		printc("robot_cont: Task Complete\n");
		if (first) cos_asnd(driver_asnd, DRIVER_AEPKEY);
		first = 1;
	}
}

void
cos_init(void)
{
	printc("\nWelcome to the robot_cont component\n");
	thdid_t tidp;
	int i = 0;
	cycles_t wakeup, now;
	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	rpos.x = 0;
	rpos.y = 0;
	rpos.direction = EAST;	
	
	printc("robot_cont aep created!\n");
	tidp = sched_aep_create(&taeps, task_complete_aep, (void *)i, 0, ROBOT_CONT_AEP_KEY);	
	assert(tidp);
	sched_thd_param_set(tidp, sched_param_pack(SCHEDP_PRIO, AEP_PRIO));
	

	/* Ensure udp server is booted before we return */
	/* Having udp init sharedmem ensures shdmem_id is hardcoded (I know- we'll fix it later) */
	memmgr_shared_page_map(0, &shmem_addr);
	while (!shmem_addr) {
		rdtscll(now);
		wakeup = now + (2000 * 1000 * cycs_per_usec);
		sched_thd_block_timeout(0, wakeup);
		
		memmgr_shared_page_map(ROBOTCONT_UDP_SHMEM_ID, &shmem_addr);
	}

//	driver_asnd = capmgr_asnd_key_create(1);
//	assert(driver_asnd);
//	cos_asnd(driver_asnd, DRIVER_AEPKEY);
	
	printc("robot_cont init done\n");
	sched_thd_block(0);
}
