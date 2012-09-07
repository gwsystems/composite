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

#include <ck_spinlock.h>

#include <pong.h>

cos_lock_t membrane_l;
#define TAKE()    do { if (unlikely(lock_take(&membrane_l) != 0)) BUG(); }   while(0)
#define RELEASE() do { if (unlikely(lock_release(&membrane_l) != 0)) BUG() } while(0)
#define LOCK_INIT()    lock_static_init(&membrane_l);

#define SYNC_INV
#ifndef SYNC_INV
#define ASYNC_INV
#endif

//#define NO_MEMBRAIN

#define IPI_TEST
#ifndef IPI_TEST
#define SHMEM
#endif

struct inv_data {
	int p1, p2, p3, p4;
	int loaded, processed, ret;
} CACHE_ALIGNED;
volatile struct inv_data inv;

volatile int t1, t2;
volatile int brand_tid;
volatile int global_cnt = 0;

ck_spinlock_ticket_t sl = CK_SPINLOCK_TICKET_INITIALIZER;
int server_receive(void)
{
#ifdef NO_MEMBRAIN
	return 0;
#endif	
	printc("Core %ld thd %d: membrane waiting...\n", cos_cpuid(), cos_get_thd_id());

	/* IPI mechanism */
#ifdef IPI_TEST
	brand_tid = cos_brand_cntl(COS_BRAND_CREATE, 0, 0, cos_spd_id());
	assert(brand_tid > 0);
	if (sched_add_thd_to_brand(cos_spd_id(), brand_tid, cos_get_thd_id())) BUG();
	int tid;
	tid = brand_tid;
        int received_ipi = 0;
	while (1) {
		int ret = 0;
		if (-1 == (ret = cos_brand_wait(tid))) BUG();
		received_ipi++;
		global_cnt++;
		if (received_ipi > 19990) printc("rec %d, global_cnt %d\n", received_ipi, global_cnt);
		inv.loaded = 0;
	}
#endif

#ifdef SHMEM
	/* Shared memory mechanism */
	while (1) { //keep spinning on shmem.
		while (inv.loaded == 0) ;
		/* assert(inv.p1 == 99); */
		call();
		//inv.ret = call();
		inv.loaded = 0;
		//inv.processed = 1;
	}
#endif
	return 0;
}

#define ITER (10000)
u64_t meas[NUM_CPU][ITER];
volatile int start[NUM_CPU] = { 0 };

int call_server(int p1, int p2, int p3, int p4)
{
	/* ck_spinlock_ticket_lock_pb(&sl); */
	/* ck_spinlock_ticket_unlock(&sl); */
#ifdef NO_MEMBRAIN
	return 0;
#endif	

	/* IPI mechanism */
#ifdef IPI_TEST
	start[cos_cpuid()] = 1;
	
	while (start[0] == 0) ;
	while (start[2] == 0) ;
	/* inv.loaded = 1; */
	/* cos_send_ipi(((cos_cpuid() + 1) % (NUM_CPU - 1)), brand_tid, 0, 0); */
	/* while (inv.loaded == 1) ; */
	/* return 0; */

	printc("Core %ld: going to send IPIs to tid %d!\n",cos_cpuid(), brand_tid);
	u64_t start = 0, end = 0, avg, tot = 0, dev = 0, max = 0;
	int i, j, t;
	int bid;
	/* printc("brandtid @ %p.\n", &brand_tid); */
	/* printc("meas from %p to %p.\n", meas, &meas[ITER-1]); */
	bid = brand_tid;
	for (i = 0 ; i < ITER ; i++) {
		rdtscll(start);
//		inv.loaded = 1;
//		t = cos_send_ipi(cos_cpuid(), bid, 0, 0);
//		t = cos_send_ipi(((cos_cpuid() + 1) % (NUM_CPU - 1 > 0 ? NUM_CPU - 1 : 1)), bid, 0, 0);
		t = cos_send_ipi(1, bid, 0, 0);
//		while (inv.loaded == 1) ;
		rdtscll(end);
		meas[cos_cpuid()][i] = end - start;
	}

	for (i = 0 ; i < ITER ; i++) {
		tot += meas[cos_cpuid()][i];
		if (meas[cos_cpuid()][i] > max) max = meas[cos_cpuid()][i];
	}
	avg = tot/ITER;
	printc("avg %lld\n", avg);
	for (tot = 0, i = 0, j = 0 ; i < ITER ; i++) {
		if (meas[cos_cpuid()][i] < avg*2) {
			tot += meas[cos_cpuid()][i];
			j++;
		}
	}
	printc("avg w/o %d outliers %lld\n", ITER-j, tot/j);

	for (i = 0 ; i < ITER ; i++) {
		u64_t diff = (meas[cos_cpuid()][i] > avg) ?
			meas[cos_cpuid()][i] - avg :
			avg - meas[cos_cpuid()][i];
		dev += (diff*diff);
	}
	dev /= ITER;
	printc("deviation^2 = %lld\n", dev);
	printc("max = %llu\n", max);
	printc("global_cnt = %d\n", global_cnt);

	return 0;
#endif

	/* Shared memory mechanism */
#ifdef SHMEM	
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
#endif
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
		sp.c.value = 10;
		if (sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();
		return;
	}
	
	server_receive();
//	memset(all_tmem_mgr, 0, sizeof(struct tmem_mgr *) * MAX_NUM_SPDS);

	return;
}
