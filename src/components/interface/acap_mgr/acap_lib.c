#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <acap_mgr.h>

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
		/* return static cap */
		return ucap->cap_no;
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
