/**
 * Copyright 2012 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Qi Wang, interwq@gwu.edu, 2012
 */

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

//#include <ck_spinlock.h>

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

#define CACHE_EFFECT
//#define IPI_TEST
//#define SHMEM

struct inv_data {
	int p1, p2, p3, p4;
	int loaded, processed, ret;
} CACHE_ALIGNED;
volatile struct inv_data inv;

volatile int t1, t2;
volatile int brand_tid;
volatile int global_cnt = 0;

#define ITER (1024*1024/64)
u64_t meas[NUM_CPU][0];

struct cache_line {
	struct cache_line *next;
	int data;
} CACHE_ALIGNED;
struct cache_line cache[ITER];

//ck_spinlock_ticket_t sl = CK_SPINLOCK_TICKET_INITIALIZER;
int server_receive(void)
{
#ifdef NO_MEMBRAIN
	return 0;
#endif	

	printc("Core %ld thd %d: membrane waiting...\n", cos_cpuid(), cos_get_thd_id());
#ifdef CACHE_EFFECT
	int k;
	assert(sizeof(struct cache_line) <= 64);
	/* Shared memory mechanism */
	while (1) {
		while (inv.loaded == 0) ;

		k = inv.p1;
		struct cache_line *node = (struct cache_line *)(inv.p2);
		int i;
		/* assert(inv.p1 == 99); */
		for (i = 0; i < k; i++) {
			node->data++;
//			if (likely(node->next > 100)) 
			node = node->next;
		}
//		call();
		//inv.ret = call();
		inv.loaded = 0;
		//inv.processed = 1;
	}

	return 0;
#endif	
	
	int i;
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
	int k;
	/* Shared memory mechanism */
	while (1) {
		while (inv.loaded == 0) ;
		k = inv.p1;
		/* assert(inv.p1 == 99); */
//		call();
		//inv.ret = call();
		inv.loaded = 0;
		//inv.processed = 1;
	}
#endif
	return 0;
}

volatile int start[NUM_CPU] = { 0 };
//#define ITER2 1000
//unsigned long cost[ITER2];
int call_server(int p1, int p2, int p3, int p4)
{
	/* ck_spinlock_ticket_lock_pb(&sl); */
	/* ck_spinlock_ticket_unlock(&sl); */
#ifdef NO_MEMBRAIN
	return 0;
#endif	
	int i, j;
#ifdef CACHE_EFFECT
	unsigned long long s,e, avg, sum = 0, sum2=0;
	assert(p1 <= ITER);
	if (!p1) return 0;
//	rdtscll(s);
	/* for (i = 0; i < p1; i ++) { */
	/* 	cache[i]++; */
	/* } */
//	int repeat, outlier = 0;
//	for (repeat=0; repeat < ITER2; repeat++) {
		struct cache_line *node = cache;
//		rdtscll(s);
		for (i = 0; i < p1; i++) {
			node->data++;
			node = node->next;
		}
//		rdtscll(e);
//		cost[repeat] = (e - s);
//		sum += cost[repeat];
		
		/* continue; */

		/* for (i = 0; i < p1; i++) { */
		/* 	node->data++; */
		/* 	node = node->next; */
		/* 	assert(node); */
		/* } */
		/* continue; */
		

		/* int ret = 0; */

		/* inv.p1 = p1; */
		/* inv.p2 = (int)node; */
		/* inv.p3 = p3; */
		/* inv.p4 = p4;  */
		/* inv.loaded = 1; */

		/* while (inv.loaded == 1) ; */
		/* ret = inv.ret; */

//	}
	/* avg = sum / repeat; */

	/* for (i = 0; i < ITER2; i++) { */
	/* 	if (cost[i] <= 2*avg) { */
	/* 		sum2 += cost[i]; */
	/* 	} else */
	/* 		outlier++; */
	/* } */
//	printc("avg %llu, sum %llu, sum2 %llu\n",avg,sum,sum2);
//	printc("Core %d, calling side, cache working set size %d, avg execution time %llu w/o %d outliers\n", cos_cpuid(), p1 * 64, sum2 / (ITER2-outlier), outlier);
	return 0;
#endif

	/* Shared memory mechanism */
#ifdef SHMEM	
	int ret = 0;
	//write to shmem.
	/* for (i = 0; i < p1; i++) { */
	/* 	cache[i]++; */
	/* } */
	/* return 0; */

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
	int t;
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

}

int call_server_x(int p1, int p2, int p3, int p4)
{
	int i, j;
#ifdef CACHE_EFFECT
	unsigned long long s,e, avg, sum = 0, sum2=0;
	assert(p1 <= ITER);

	inv.p1 = p1;
	inv.p2 = (int)cache;
	inv.p3 = p3;
	inv.p4 = p4;
	inv.loaded = 1;

	while (inv.loaded == 1) ;

	return 0;
#endif
}

int register_inv(void) // function, sync / async, # of params, return value...
{
	// might be complicated
	return 0;
}

void init_cache(void)
{
	struct cache_line *l = cache, *temp;
	int i,j, k = 1;
	unsigned long long random;
	l->data = 1;
	for (i = 0; i < ITER - 1; i++) {
		rdtscll(random);
		random /= 4;
		temp = &cache[random % ITER];

		if (temp->data == 0) {
			l->next = temp;
			l = l->next;
			l->data = 1;
			k++;
//			printc("%llu..%d, %d\n", random, random % ITER, k);
		} else {
			i--;
			continue;
		}
	}

	for (i = 0; i < ITER; i++) {
		if (!cache[i].data) printc("i %d\n", i);
	//assert(cache[i].data);
	}
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

		init_cache();
		LOCK_INIT();
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 30;
		if (sched_create_thd(cos_spd_id(), sp.v, 0, 0) == 0) BUG();
		return;
	}
	
	server_receive();
//	memset(all_tmem_mgr, 0, sizeof(struct tmem_mgr *) * MAX_NUM_SPDS);

	return;
}
