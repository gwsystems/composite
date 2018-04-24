#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <cos_alloc.h>
#include <sched.h>

//int shmid;
//vaddr_t shm_vaddr; 
//
//int
//assign_task(unsigned long token, int x, int y) {
//	printc("Assign task in robot sched\n");
//	printc("Car_mgr %d requested task: (%d,%d)  \n", token, x, y); 
//
//	send_task(x, y);
//
//	return 1;
//}
void
cos_init(void)
{
	printc("\nWelcome to the robot sched component\n");

//	shmid = shm_map(4, 0);	
//
//	printc("shmid: %d", shmid);
//	shm_vaddr = shm_get_vaddr(4, shmid);
//	printc("shm_vaddr: %p\n", shm_vaddr);

//	printc("test: %s \n", (char *)shm_vaddr);


	sched_thd_block(0);
	return;
}


