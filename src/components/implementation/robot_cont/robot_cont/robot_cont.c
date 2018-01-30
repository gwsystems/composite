#include <llprint.h>

#include <robot_cont.h>
#include <shdmem.h>
#include <posix.h>
#include <sl.h>
#include <sl_lock.h>
#include <sl_thd.h>

#include <locale.h>
#include <limits.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <camera.h>
#define NORTH 0
#define EAST 1
#define SOUTH 2
#define WEST 3

int shmid;
vaddr_t shm_vaddr;

struct rp {
	int x, y;
	unsigned long direction;
};
struct rp rpos;

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
		for (i = 0; i < sidx ; i ++) {
			printc("%u ", script[i]);
		}
		printc("\n");

//	} else if (ychange < 0) {
//		/* turn south */
//		/* check angle */
//	}

	rpos.x = xf;
	rpos.y = yf;	
	
	return 0;
}

int
send_task(int x, int y) {

	int position;

	create_movement(x, y);

	printc("Send script to udp\n");
	
	printc("Checking location via camera: \n");
	position = check_location_image(x, y);

	if (position) {
		printc("Camera in correct location\n");
	}

	printc("new position: %d, %d\n", rpos.x, rpos.y);
	printc("\n");
	
	return 0;
}

void
cos_init(void)
{
	printc("\nWelcome to the robot_cont component\n");
	
	rpos.x = 0;
	rpos.y = 0;
	rpos.direction = EAST;	

	char * test = "Hello";
	shmid = shm_allocate(3, 1);
	printc("shmid: %d \n", shmid);
	shm_vaddr = shm_get_vaddr(3, 0);
	printc("shm_vaddr: %p \n", shm_vaddr);

	memcpy(shm_vaddr, test, 5);

	cos_sinv(BOOT_CAPTBL_SINV_CAP, INIT_DONE, 2, 3, 4);
}
