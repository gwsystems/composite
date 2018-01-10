#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <robot_cont.h>
#include <malloc.h>
//#include <robot_sched.h>
#include <cos_types.h>

#define ARB_NUM 1

int* side(int distance, int* cmds) {
	int c[6] =  {132, 137 };//132, 137, FIX THIS 
	cmds = c;
	return cmds;
}

int* up(int distance, int* cmds) {
	int velocity[2];
	if (distance < 0) {
		velocity[0] = 255;
		velocity[1] = 56;
	} else {
		velocity[0] = 0; //high byte <-- not sure this is right: 200ms/s ->hex = C8
		velocity[1] = 200; //low byte
	}
	int c[6] = {132, 137, velocity[0], velocity[1], 0, 0}; //132, 137, 		
	cmds = c;
	return cmds;
}
/*int**/
void diagonal(int dx, int dy) {
}
void* determine_steps(int x, int y, int xc, int yc) {
	//assuming front is always north
	//if yc-y is negative, need to move north(forward)
	//if xc-x is negative, need to move right
	int up_var;
	int side_var; //right is positive, left is negative
	up_var = yc-y;
	side_var = xc-x;
	int cmds[6];
	if(up_var  != 0 && side_var != 0) diagonal(side_var, up_var);
	else if (up_var) return up(up_var, cmds);
	
	else if (side_var) return side(side_var, cmds);
	return 0;
}
int
send_cmd(int x) {
	printc("In send_cmd() in robot cont interface\n");
	return 1;
}
int
send_task(int *curr, struct Task *task) { //WIP change parameters. Takes ina series of cmds
	printc("In send_task() in robot cont interface\n");
	int *queue = determine_steps(task->coords[0], task->coords[1], curr[0], curr[1]);
	int i;
	int cmd_queue[2] = {136, 9}; //each contains an integer command
	for(i = 0; i < ARB_NUM; i++) {
	    send_cmd(cmd_queue[i]);
	}
	return 1;
}


int*
get_loc(void) {
	printc("In get_loc() in robot cont interface\n");
	//request image?
	static int coords[2] = {0,0};
	return coords;
}

int
receive_task(struct Task* task) { //break task down into single command or set	
	if (task->command != 0) {
	    send_cmd(task->command);
	} else if (task->coords != NULL) {
	    int *curr_loc = get_loc();
	    send_task(curr_loc, task);
	}
	printc("In receive_task() in robot cont interface\n");
	return 1;
}

void
cos_init(void)
{
	int ret;

	printc("Welcome to the robot cont component\n");
	
	cos_sinv(BOOT_CAPTBL_SINV_CAP, 1, 2, 3, 4);
}
