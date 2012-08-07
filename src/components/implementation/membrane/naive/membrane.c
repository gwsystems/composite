#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_list.h>
#include <cinfo.h>
#include <sched.h>
#include <mem_mgr_large.h>

#include <valloc.h>

#include <cos_vect.h>
#include <cos_synchronization.h>
cos_lock_t membrane_l;
#define TAKE()    do { if (unlikely(lock_take(&membrane_l) != 0)) BUG(); }   while(0)
#define RELEASE() do { if (unlikely(lock_release(&membrane_l) != 0)) BUG() } while(0)
#define LOCK_INIT()    lock_static_init(&membrane_l);

#include <pong.h>
#define SYNC_INV
#ifndef SYNC_INV
#define ASYNC_INV
#endif

struct inv_data {
	int p1, p2, p3, p4;
	int loaded, processed, ret;
} CACHE_ALIGNED;

volatile struct inv_data inv;

#define NO_MEMBRAIN

int server_receive(void)
{
#ifdef NO_MEMBRAIN
	return 0;
#endif	
	printc("Core %ld: membrane waiting...\n", cos_cpuid());

	while (1) {//keep spinning on shmem.
		while (inv.loaded == 0) ;
		/* assert(inv.p1 == 99); */
		/* assert(inv.p2 == 99); */
		/* assert(inv.p3 == 99); */
		/* assert(inv.p4 == 99); */
		call();
		//inv.ret = call();
		inv.loaded = 0;
		//inv.processed = 1;
	}

	return 0;
}

int call_server(int p1, int p2, int p3, int p4)
{
#ifdef NO_MEMBRAIN
	return 0;
#endif	
	int ret = 0;
	//write to shmem.
	inv.p1 = p1;
	inv.p2 = p2;
	inv.p3 = p3;
	inv.p4 = p4; 
	//inv.processed = 0;
	inv.loaded = 1;

#ifdef SYNC_INV
	while (inv.loaded == 1) ;
	ret = inv.ret;
	//reading the return value
#endif

	return ret;
}

int register_inv(void) // function, sync / async, # of params, return value...
{
	// might be complicated
	return 0;
}

/**
 * cos_init
 */
void
cos_init(void *arg)
{
	static int first = 1;

	if (first) {
		union sched_param sp;
		first = 0;

		LOCK_INIT();
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 31;
		if (sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();
		return;
	}
	
	server_receive();
//	memset(all_tmem_mgr, 0, sizeof(struct tmem_mgr *) * MAX_NUM_SPDS);

	return;
}
