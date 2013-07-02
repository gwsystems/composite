#include <cos_component.h>
#include <print.h>

#include <sched.h>
#include <acap_pong.h>

#include <acap_mgr.h> 
#include <cos_alloc.h> 

#define ITER (1024)
u64_t meas[ITER];

//////////////////////////////// move to lib

struct cap_info {
	int acap;
	int static_inv;
	void *shared_page;
	struct shared_struct shared_struct;
};

struct ainv_info {
	int thdid; // owner thread
	struct cap_info *cap[MAX_STATIC_CAP]; // static cap to acap mapping
} CACHE_ALIGNED;

struct ainv_info *thd_ainv[MAX_NUM_THREADS]; // TODO: replace with cvect 

int cos_async_inv(struct usr_inv_cap *ucap, int *params) {
	int cap_id = ucap->cap_no >> COS_CAPABILITY_OFFSET;
	int acap, ret, curr_id = cos_get_thd_id();
	struct ainv_info *curr;
	struct cap_info *curr_cap;

	curr = thd_ainv[curr_id];
	if (unlikely(curr == NULL)) {
		thd_ainv[curr_id] = malloc(sizeof(struct ainv_info));
		curr = thd_ainv[curr_id];
		if (unlikely(curr == NULL)) goto err_nomem;
		curr->thdid = cos_get_thd_id();
	}
	assert(curr);

	curr_cap = curr->cap[cap_id];
	if (unlikely(curr_cap == NULL)) {
		curr->cap[cap_id] = malloc(sizeof(struct cap_info));
		curr_cap = curr->cap[cap_id];
		if (unlikely(curr_cap == NULL)) goto err_nomem;
	}
	assert(curr_cap);
	
	acap = curr_cap->acap;
	if (unlikely(acap == 0 && curr_cap->static_inv == 0)) {
		// call acap mgr
		acap = acap_cli_lookup(cos_spd_id(), cap_id);
		/* printc("Client acap lookup %d\n", acap); */
		if (acap != 0) {
			curr_cap->acap = acap;
			curr_cap->shared_page = acap_cli_lookup_ring(cos_spd_id(), cap_id);
			assert(curr_cap->shared_page);
			init_shared_page(&curr_cap->shared_struct, curr_cap->shared_page);
		} else {
			/* do static for current thread */
			curr_cap->static_inv = 1;
		}
	}

	if (curr_cap->static_inv == 1) {
		// return static cap
	}

	struct shared_struct *shared_struct = &curr_cap->shared_struct;
	assert(shared_struct->ring);

        printc("core %ld: sending to static cap %d, params %d, %d, %d, %d, acap %d\n",
	       cos_cpuid(), cap_id, *params, *(params+1), *(params+2), *(params+3), acap);

	struct inv_data inv;
	inv.cap = cap_id;
	memcpy(inv.params, params, sizeof(int) * 4); //figure out num of params?

	/* Write to the ring buffer. Spin when buffer is full. */
	while (unlikely(!CK_RING_ENQUEUE_SPSC(inv_ring, shared_struct->ring, &inv))) ;

	/* decide whether we should send ipi. return 0 if not. */
	if (SERVER_ACTIVE(shared_struct)) return 0;

	return acap;
err_nomem:
	return -1;
}

//////////////////////////////// above move to lib




void cos_init(void)
{
	u64_t start, end, avg, tot = 0, dev = 0;
	int i, j;

	call(111,222,333,444);			/* get stack */
	printc("cpu %ld, thd %d from ping\n",cos_cpuid(), cos_get_thd_id());
	printc("Starting %d Invocations.\n", ITER);

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
