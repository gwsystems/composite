#include <cos_kernel_api.h>
#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <memmgr.h>
#include <udpserver.h>
#include <sched.h>
#include <capmgr.h>

#include <robot_cont.h>
#include <gateway_spec.h>

#define DRIVER_AEPKEY 1

#define NORTH 0
#define EAST 1
#define SOUTH 2
#define WEST 3
asndcap_t driver_asnd;

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
	
	/* roomba scripts can be 100 bytes long */
	/* copy generated script into shared memory for the udpserver, and notify it of an update */

	unsigned char script[100] = { 152, 91,
                    137, 1, 44, 0, 1, 157, 0, 88, 137, 0, 0, 0, 0,
                    137, 1, 44, 128, 0, 156, 1, 144, 137, 0, 0, 0, 0,
                    137, 1, 44, 0, 1, 157, 0, 85, 137, 0, 0, 0, 0,
                    137, 1, 44, 128, 0, 156, 3, 32, 137, 0, 0, 0, 0,
                    137, 1, 44, 128, 0, 156, 3, 32, 137, 0, 0, 0, 0,
                    137, 1, 44, 0, 1, 157, 0, 85, 137, 0, 0, 0, 0,
                    137, 1, 44, 128, 0, 156, 1, 144, 137, 0, 0, 0, 0, SCRIPT_END, 0,0,0,0,0,0
                  };	

	printc("shmem: %p \n", (char *) shmem_addr);
	memcpy((unsigned char *)shmem_addr, script, 100);	
	udpserv_script(93);
	
	rpos.x = xf;
	rpos.y = yf;	
	
	return 0;
}

int
send_task(int x, int y) 
{
	printc("send_task\n");
	if (!shmem_addr) return -1;
	
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
	cycles_t wakeup, now, cycs_per_usec;
	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	rpos.x = 0;
	rpos.y = 0;
	rpos.direction = EAST;	
	
//	thdid_t tiddumb;
//	tiddumb = sched_aep_create(&taepsdumb, task_complete_aep, (void *)1, 0, ROBOT_CONT_AEP_KEY);	

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

	driver_asnd = capmgr_asnd_key_create(1);
	assert(driver_asnd);
	cos_asnd(driver_asnd, DRIVER_AEPKEY);

	printc("robot_cont init done\n");
	sched_thd_block(0);
}
