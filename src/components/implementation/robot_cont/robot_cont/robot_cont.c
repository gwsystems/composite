#include <cos_kernel_api.h>
#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <memmgr.h>
#include <udpserver.h>
#include <sched.h>

#include <robot_cont.h>

#define NORTH 0
#define EAST 1
#define SOUTH 2
#define WEST 3

struct rp {
	int x, y;
	unsigned long direction;
};
struct rp rpos;

vaddr_t shmem_addr = NULL;

int
update_script(int x)
{
	printc("Updating script! \n");
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
                    137, 1, 44, 128, 0, 156, 1, 144, 137, 0, 0, 0, 0, 66, 0,0,0,0,0,0
                  };	
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
	
	return 0;
}

void
cos_init(void)
{
	printc("\nWelcome to the robot_cont component\n");
	int ret;
	cycles_t wakeup, now, cycs_per_usec;
	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	rpos.x = 0;
	rpos.y = 0;
	rpos.direction = EAST;	
	
	ret = memmgr_shared_page_map(0, &shmem_addr);

	/* Ensure udp server is booted before we return */
	while (!shmem_addr) {
		ret = memmgr_shared_page_map(0, &shmem_addr);
		if (shmem_addr != NULL) break;
		rdtscll(now);
		wakeup = now + (2000 * 1000 * cycs_per_usec);

		sched_thd_block_timeout(0, wakeup);
	}
	
	printc("robot_cont init done\n");
	sched_thd_block(0);
}
