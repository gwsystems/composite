#include <cos_component.h>
#include <print.h>

#include <sched.h>
#include <acap_pong.h>

#include <acap_mgr.h> 
#include <cos_alloc.h> 

#define ITER (1024)
u64_t meas[ITER];

struct ainv_cap {
	int thdid; // owner thread
	int acap_mapping[MAX_NUM_ACAP]; // static cap to acap
} CACHE_ALIGNED;

struct ainv_cap *thd_ainv[MAX_NUM_THREADS];

int cos_async_inv(struct usr_inv_cap *ucap, int *params) {
	/* cap_v is the static capability value */
	
	int cap = ucap->cap_no >> COS_CAPABILITY_OFFSET;
	int acap, curr = cos_get_thd_id();

        printc("params (static cap %d), %d, %d, %d, %d\n",
	cap, *params, *(params+1), *(params+2), *(params+3));

	if (unlikely(thd_ainv[curr] == NULL)) {
		thd_ainv[curr] = malloc(sizeof(struct ainv_cap));
		if (unlikely(thd_ainv[curr] == NULL)) goto err_nomem;
		thd_ainv[curr]->thdid = cos_get_thd_id();
	}
	
	acap = thd_ainv[curr]->acap_mapping[cap];
	if (unlikely(acap == 0)) {
		// call acap mgr
		acap = acap_cli_lookup(cos_spd_id(), cap);
		assert(acap != 0);
		thd_ainv[curr]->acap_mapping[cap] = acap;
	}

	// push into ring buffer

	// decide whether should send ipi. set acap = 0 if not.

	return acap;
err_nomem:
	return -1;
}




void cos_init(void)
{
	u64_t start, end, avg, tot = 0, dev = 0;
	int i, j;

	call(0,0,0,0);			/* get stack */
	printc("cpu %ld, thd %d from ping\n",cos_cpuid(), cos_get_thd_id());
	printc("Starting %d Invocations.\n", ITER);

	for (i = 0 ; i < ITER ; i++) {
		rdtscll(start);
//		cos_send_ipi(i, 0, 0, 0);
		call(0,0,0,0);
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

	sched_block(cos_spd_id(), 0);
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
