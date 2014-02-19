#include <cos_component.h>
#include <print.h>

#include <sched.h>
#include <acap_pong.h>

#include <par_mgr.h> 
#include <cos_alloc.h> 

#define ITER (1024)
u64_t meas[ITER];

#include <parlib.h>

void delay(void)
{
	int i,j,k;
	volatile int m = 0;

	for (i = 0; i < 1000; i++)
		for(j = 0; j < 100; j++)
			for (k = 0; k < 100; k++)
				m = 123;
}

void test_fn(int *data) {
	printc("Working fn: core %ld, thd %d (thd num %d), data0 %d\n",
	       cos_cpuid(), cos_get_thd_id(), ainv_get_thd_num(), *(data + 4*ainv_get_thd_num()));
	
	return;
}

void cos_init(void)
{
	u64_t start, end, avg, tot = 0, dev = 0;
	int i, j;

	return;
	printc("cpu %ld, thd %d from ping\n",cos_cpuid(), cos_get_thd_id());
//	call(111,222,333,444);			/* get stack */

	printc("core %ld: spinning....\n", cos_cpuid());
	delay();
	printc("core %ld: after spin!\n", cos_cpuid());
	
//	call(1111,2222,3333,4444);			/* get stack */
	printc("Starting %d Invocations.\n", ITER);
	
	int params[8] = {1,2,3,4, 11, 22, 33, 44};
	par_inv(test_fn, (void *)params, 2);

	return;

	for (i = 0 ; i < ITER ; i++) {
		rdtscll(start);
//		cos_send_ipi(i, 0, 0, 0);
		call(1,2,3,4);
		rdtscll(end);
		meas[i] = end-start;
	}

	for (i = 0 ; i < ITER ; i++) tot += meas[i];
	avg = tot/ITER;
	printc("avg %lld\n", avg);
	for (tot = 0, i = 0, j = 0 ; i < ITER ; i++) {
		if (meas[i] < avg*2) {
			tot += meas[i];
			j++;
		}
	}
	printc("avg w/o %d outliers %lld\n", ITER-j, tot/j);

	for (i = 0 ; i < ITER ; i++) {
		u64_t diff = (meas[i] > avg) ? 
			meas[i] - avg : 
			avg - meas[i];
		dev += (diff*diff);
	}
	dev /= ITER;
	printc("deviation^2 = %lld\n", dev);

	printc("last invocation...\n");
	rdtscll(start);
	int rrr = call(11,22,33,44);
	rdtscll(end);
	printc("done ret %d. cost %llu \n", rrr, end-start);

	rdtscll(start);
	rrr = call(11,22,33,44);
	rdtscll(end);
	printc("done ret %d. cost %llu \n", rrr, end-start);

//	printc("%d invocations took %lld\n", ITER, end-start);
	return;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	static int init = 0;
	switch (t) {
	case COS_UPCALL_THD_CREATE:
	{
		if (init == 0) {//  add a sched type!
			init = 1;
			cos_init();
		} else {
			cos_intra_ainv_handling();
		}
		break;
	}
	default:
		/* fault! */
		//*(int*)NULL = 0;
		printc("\n upcall type t %d\n", t);
		return;
	}
	return;
}
