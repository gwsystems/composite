#include <cos_kernel_api.h>
#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <sched.h>
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
	unsigned char script [100];
	int sidx = 0;
	script[sidx++] = 152; // Indicates we're sending a script
	/* script[1] is reserved for script length, will populate after we generate the script */
	sidx++;

//	/* Must move north */	
//	if (ychange > 0) {

		switch (rpos.direction) {
			case NORTH:
			{
				break;
			}
			case EAST:
			{
				/* Turn 90 counter clock */
				
				/* drive */
				script[sidx++] = 137;
				
				/* 300 mm/s */
				script[sidx++] = 1;
				script[sidx++] = 44;
				
				/* Spin counter clock */
				script[sidx++] = 0;
				script[sidx++] = 1;
				
				/* Wait for angle */
				script[sidx++] = 157;
				
				/* 90 degrees */
				script[sidx++] = 0;
				script[sidx++] = 90;
				
				break;
			}
			case SOUTH:
			{
				/* Turn 180 counter clock */
				
				/* Drive */
				script[sidx++] = 137;
				
				/* 300 mm/s */
				script[sidx++] = 1;
				script[sidx++] = 44;
				
				/* Spin counter clock */
				script[sidx++] = 0;
				script[sidx++] = 1;
				
				/* Wait for angle */
				script[sidx++] = 157;
				
				/* 90 degrees */
				script[sidx++] = 0;
				script[sidx++] = 180;
				
				break;
			}
			case WEST:
			{
				/* Turn 90 clock */
				/* Drive */
				script[sidx++] = 137;
				
				/* 300 mm/s */
				script[sidx++] = 1;
				script[sidx++] = 44;
				
				/* Spin counter clock */
				script[sidx++] = 0;
				script[sidx++] = 0;
				
				/* Wait for angle */
				script[sidx++] = 157;
				
				/* 90 degrees */
				script[sidx++] = 0;
				script[sidx++] = 90;

				break;
			}
			default:
				printc("error, no direction?\n");
				break;
		}
		/* move forward 100mm * ychange */
		//137 1 44 128 0 156 1 144
		/* Drive */
		script[sidx++] = 137;
		
		/* 300 mm/s */
		script[sidx++] = 1;
		script[sidx++] = 44;
	
		/* Straight */	
		script[sidx++] = 128;
		script[sidx++] = 0;
	
		/* Wait for distance */	
		script[sidx++] = 156;

		/* Distance == 100mm * ychange */	
		script[sidx++] = 1;
		script[sidx++] = 36 * ychange;

		/* start script */		
		script[sidx] = 153;	
		script[1] = sidx - 1;

//		printc("script length: %d: \n", sidx - 1);
	//	for (i = 0; i < sidx ; i ++) {
	//		printc("%u ", script[i]);
	//	}
		printc("\n");

//	} else if (ychange < 0) {
//		/* turn south */
//		/* check angle */
//	}

	/* copy generated script into shared memory for the udpserver, and notify it of an update */
	memcpy((unsigned char *)shmem_addr, script, sidx);	
	udpserv_script(sidx);
	
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
