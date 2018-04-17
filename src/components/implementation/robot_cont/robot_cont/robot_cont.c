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
#define SCRIPT_START 152
#define MAX_APPS 5 //TODO: arbitrary number for now

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
int blacklisted[MAX_APPS];
unsigned char script[100];

int
update_script(int x)
{
	return 0;
}

int
blacklist(int token) {
	blacklisted[token] = 1;
	return 0;
}

int
un_blacklist(int token) {
	/* it's safe now
	Ideally called by robot_sched */
	blacklisted[token] = 0;
	return 0;
}

int
check_legality(int x, int y) {
/* super simple bounds check */
	if(x < 0 || x > 2 || y < 0 || y > 2) return 0;
	return 1;
}

int
detoury (int* i, int** orange, unsigned long* dir_array,int  xcurr, int ycurr, int xf, int yf)
{
//since xchange >0 and obstacle at xcurr+1 and ycurr
                int j = *i;
                if(yf-ycurr <= 0 && ycurr !=0) { //move NORTH
                         switch(rpos.direction) {
                                printc("detoury\n");

                                case NORTH: { dir_array[j] = NORTH; break; }
                                case EAST: {dir_array[j] = WEST; break; }
                                case SOUTH: { dir_array[j] = SOUTH; break;}
                                case WEST: { dir_array[j] = EAST; break; }
                                                default:
                                printc("error, no direction?\n");
                                break;
                        }
                        rpos.direction = (rpos.direction + dir_array[j])%4;
                        ycurr--;
                        j++;
                } else {        //godown        
                         switch(rpos.direction) {
                                printc("detoury2\n");

                                case NORTH: { dir_array[j] = SOUTH; break; }
                                case EAST: {dir_array[j] = EAST; break; }
                                case SOUTH: { dir_array[j] = NORTH; break;}
                                case WEST: { dir_array[j] = WEST; break; }
                                                default:
                                printc("error, no direction?\n");
                                break;

                        }
                        rpos.direction = (rpos.direction + dir_array[j])%4;
                        ycurr++;
                        j++;

                }
                *i = j;
        return ycurr;
}


unsigned long*
generate_path(int xchange, int ychange, int xf, int yf)
{
/*
00 10 20
01 11 21
02 12 22
*/
	int obstacles[9] = {0,0,0,0,0,0,0,0,0};
	int l;
	int orange[3][3] = {{0,0,0},{0,0,0},{0,0,0}};

    int j = ret_obstacles();
    if(j!= -1) obstacles[j] = 1;
    //map Section policy to x,y policy

	unsigned long *dir_array =  (unsigned long*) malloc(6*sizeof(unsigned long));
	memset(dir_array, 5, 6); //arbitrary number that will never appear

    orange[0][0]=obstacles[0];
    orange[1][0]=obstacles[1];
    orange[2][0]=obstacles[2];

    orange[0][1]=obstacles[3];
    orange[1][1]=obstacles[4];
    orange[2][1]=obstacles[5];

    orange[0][2]=obstacles[6];
    orange[1][2]=obstacles[7];
    orange[2][2]=obstacles[8];

    int xcurr = rpos.x;
    int ycurr = rpos.y;
    int i = 0;
    int w = 0;
    printc("xcurr %d ycurr %d xf %d yf %d direction %d\n", xcurr, ycurr, xf, yf, rpos.direction);

	while(xcurr != xf || ycurr != yf) {
                if(w > 6) break;
                w++;
                xchange = xf - xcurr;
                ychange = yf - ycurr;
                printc("%d %d\n", xchange, ychange);
                if(xchange > 0 && !orange[xcurr+1][ycurr]) {
                        printc("%d\n", rpos.direction);
                        switch(rpos.direction) {
                                case NORTH: { dir_array[i] = EAST; break; }
                                case EAST: { dir_array[i] = NORTH;
                                                break; }
                                case SOUTH: { dir_array[i] = WEST; break;}
                                case WEST: { dir_array[i] = SOUTH; break; }
                                default:
                                printc("error, no direction?\n");
                                break;          }
                        rpos.direction = (rpos.direction+ dir_array[i])%4;
                        i++;
                        xcurr++;
                        continue;
                }
                if(xchange < 0 && !orange[xcurr-1][ycurr]) { //go w
                        switch(rpos.direction) {
                                case NORTH: { dir_array[i] = WEST; break; }
                                case EAST: {dir_array[i] = SOUTH; break; }
                                case SOUTH: { dir_array[i] = EAST; break;}
                                case WEST: { dir_array[i] = NORTH; break; }
                                        default:
                                printc("error, no direction?\n");
                                break;  }
                        rpos.direction = (rpos.direction + dir_array[i])%4;
                        i++;
                        xcurr--;
                        continue;
                }
	  		    if(ychange < 0 && !orange[xcurr][ycurr-1]) { //go n
                        switch(rpos.direction) {

                                case NORTH: { dir_array[i] = NORTH; break; }
                                case EAST: {dir_array[i] = WEST; break; }
                                case SOUTH: { dir_array[i] = SOUTH; break;}
                                case WEST: { dir_array[i] = EAST; break; }
                                                default:
                                printc("error, no direction?\n");
                                break;}
                        rpos.direction = (rpos.direction + dir_array[i])%4;
                        i++;
                        ycurr--;
                        continue;
                }//if not full: dir_array[something+1] == -1;
                if(ychange > 0 && !orange[xcurr][ycurr+1]) { //go s
                        switch(rpos.direction) {
                                case NORTH: { dir_array[i] = SOUTH; break; }
                                case EAST: {dir_array[i] = EAST; break; }
                                case SOUTH: { dir_array[i] = NORTH; break;}
                                case WEST: { dir_array[i] = WEST; break; }
                                        default:
                                printc("error, no direction?\n");
                                break;  }
                        rpos.direction = (rpos.direction + dir_array[i])%4;              
                        i++;
                        ycurr++;
                        continue;
                }
                if (xchange> 0 && orange[xcurr+1][ycurr]) {
                        ycurr = detoury(&i, orange, dir_array, xcurr, ycurr, xf, yf);
                        break;
                }
                if (xchange< 0 && orange[xcurr-1][ycurr]) {
                        ycurr = detoury(&i, orange, dir_array, xcurr, ycurr, xf, yf);
                }

        }
       dir_array[i] = -1;
       return dir_array;
}
    
int
generate_script(unsigned long* direction) {

	/*All directions operate relative to the robot's "current" position*/
	int east[8] = {137,1,44,255,255,157,255,166};
	int south[8] = {137,1,44,0,1,157,0,180};
	int west[8] = {137,1,44,0,1,157,0,90};

	 /*north-south*/
	int straight_ns[8] = {137,1,44,128,0,156,1,144}; 

	/*east-west*/
	int straight_ew[8] = {137,1,44,128,0,156,3,232};

	int stop[5] = {137,0,0,0,0};

	unsigned long currd = rpos.direction;

	int i;
	int j = 0;
	int sidx = 0;
	
	script[sidx] = SCRIPT_START;
	/* leave space for size of script */
	sidx+=2;

	while((int)direction[j] != -1) {
		switch (direction[j]) {
			case NORTH:
			{
				printc("NORTH\n");
				break;
			}
			case EAST:
			{
				printc("EAST\n");
				memcpy(&script[sidx], east, 8);
				sidx+=8;
				break;
			}
			case SOUTH:
			{
				printc("SOUTH\n");
				memcpy(&script[sidx], south, 8);
				sidx+=8;
				break;	
			}
			case WEST:
			{
				printc("WEST\n");
				memcpy(&script[sidx], west, 8);
				sidx+=8;	
				break;
			}
			default:
				printc("error, no direction?\n");
				break; 	
		}	
		/*If north, means move straight forward.
		Either way, need to move straight after turning*/
		if (direction[j] == NORTH || direction[j] == SOUTH) {
			printc("STRAIGHT N-S\n");
			memcpy(&script[sidx], straight_ns, 8);
			sidx+=8;
		} else if (direction[j] == EAST || direction[j] == WEST) {
			printc("STRAIGHT E-W\n");
			memcpy(&script[sidx], straight_ew, 8);
			sidx+=8;
		}
		
		/*Then make sure to stop*/
		memcpy(&script[sidx], stop, 5);
		sidx+=5;
		j++;	
	}
	/* start script*/
	script[sidx] = 153; //TODO: make SCRIPT_END equal 153
	script[1] = sidx-2;

	/* print generated script*/
	for(i = 0; i < sidx; i++) {
		printc("%u ", script[i]);
	}

	return 0;
}


int
create_movement(int xf, int yf) {

	int ychange = yf - rpos.y;
	int xchange = xf - rpos.x;
	int i;
	cycles_t wakeup, now;
	
	/* roomba scripts can be 100 bytes long */
	/* copy generated script into shared memory for the udpserver, and notify it of an update */
/*
	unsigned char script[100] = { 152, 91,
                    137, 1, 44, 0, 1, 157, 0, 88, 137, 0, 0, 0, 0,
                    137, 1, 44, 128, 0, 156, 1, 144, 137, 0, 0, 0, 0,
                    137, 1, 44, 0, 1, 157, 0, 85, 137, 0, 0, 0, 0,
                    137, 1, 44, 128, 0, 156, 3, 32, 137, 0, 0, 0, 0,
                    137, 1, 44, 128, 0, 156, 3, 32, 137, 0, 0, 0, 0,
                    137, 1, 44, 0, 1, 157, 0, 85, 137, 0, 0, 0, 0,
                    137, 1, 44, 128, 0, 156, 1, 144, 137, 0, 0, 0, 0, SCRIPT_END, 0,0,0,0,0,0
                  };	
*/
	
	/* reset script global */
	memset(script, 0, 100);

	unsigned long *direction = (unsigned long*)malloc(6*sizeof(unsigned long));

	/* generate initial path from x,y coords to xf,yf coords */
	direction = generate_path(xchange, ychange, xf, yf);

	/* put script into global script */
	generate_script(direction);

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

	printc("shmem: %p \n", (char *) shmem_addr);
	memcpy((unsigned char *)shmem_addr, script, 100);	
	udpserv_script(SEND_SCRIPT, 8);
	
	rpos.x = xf;
	rpos.y = yf;	
	
	return 0;
}

int
send_task(int token, int x, int y) 
{
	/* illegal action */
	if(!check_legality(x, y)) {
		/* do not execute, error */
		blacklist(token);
		return -1;
	}
	/* if this app is already blacklisted */
	if(blacklisted[token]) return -1;

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
