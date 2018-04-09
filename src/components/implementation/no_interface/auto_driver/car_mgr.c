#include <cos_kernel_api.h>
#include <cos_component.h>
#include <sched.h>
#include <robot_cont.h>

#include <memmgr.h>

void
car_main(void)
{
	printc("car main, blocking.\n");

	cycles_t wakeup, now, cycs_per_usec;
	vaddr_t shmem_addr;
	int shmem_id;

	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	shmem_id = memmgr_shared_page_alloc(&shmem_addr);	
	assert(shmem_id > -1 && shmem_addr > 0);
	printc("shdmemid: %d\n", shmem_id);

	char * string = "testing";

	memcpy((char *)shmem_addr, string, 7);

	while(1) {
		rdtscll(now);
		wakeup = now + (5000 * 1000 * cycs_per_usec);
		printc("car main\n");
		sched_thd_block_timeout(0, wakeup);
		send_task(3,2);
	}

}

void cos_init(void)
{
	printc("\n--------------- Welcome to the car_mgr component -------\n");
	car_main();	
	
	assert(0);
}
