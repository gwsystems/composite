#ifndef PARALLEL_INV_H
#define PARALLEL_INV_H

#include <acap_mgr_intra.h>

//#define 
struct intra_cap_info {
	int acap;
	void *shared_page;
	struct intra_shared_struct shared_struct;
};

struct intra_ainv_info {
//	int thdid;
	int thd_num; /* thread number in omp. 0 means main thread. */
	int n_acap;
	struct intra_cap_info *cap; /* only valid in the main thread. */

	int wait_acap, wakeup_acap;
	struct intra_ainv_info *parent;
	volatile int finished CACHE_ALIGNED; // FIXME!!! NESTED!!!
};

extern struct intra_ainv_info *thd_intra_ainv[MAX_NUM_THREADS];

static inline int ainv_parallel_start(void *fn, void *data, int iter) {
	int curr_thd_id = cos_get_thd_id();
	struct intra_ainv_info *curr_thd = thd_intra_ainv[curr_thd_id];
	struct intra_cap_info *curr_cap;
	struct intra_inv_data inv;
	struct intra_shared_struct *shared_struct;
	int i, n_acap, n = 0, acap_idx;

	if (unlikely(curr_thd == NULL)) {
		thd_intra_ainv[curr_thd_id] = malloc(sizeof(struct intra_ainv_info));
		curr_thd = thd_intra_ainv[curr_thd_id];
		if (unlikely(curr_thd == NULL)) goto err_nomem;

		n_acap = ainv_intra_create(cos_spd_id(), iter - 1, fn);
		printc("got # of acap: %d\n", n_acap);
		if (n_acap < 0) {
			printc("cos: Intra-comp ainv creation failed.\n");
			return -1;
		}
		if (n_acap > 0) {
			curr_thd->wait_acap = ainv_intra_wait_acap_create(cos_spd_id());
			curr_thd->wakeup_acap = ainv_intra_wakeup_acap_lookup(cos_spd_id());
			assert (curr_thd->wakeup_acap && curr_thd->wait_acap);
			printc("main thd %d has wait acap %d and wakeup acap %d\n", 
			       curr_thd_id, curr_thd->wait_acap, curr_thd->wakeup_acap);
		}
	}

	curr_thd->finished = 0;
	if (unlikely(curr_thd->cap == NULL)) {
		/* When we have nested fork/join, curr_thd->cap could
		 * be NULL while curr_thd isn't. */
		curr_thd->cap = malloc(sizeof(struct intra_ainv_info) * n_acap);
		if (unlikely(curr_thd->cap == NULL)) goto err_nomem;
		curr_thd->n_acap = n_acap;
		curr_thd->thd_num = 0; /* main thread. */
	}
	assert(curr_thd && curr_thd->cap);

	for (i = 0; i < iter && n_acap > 0; i++) { // sending to other cores
		if (i % (n_acap + 1) == 0) continue;

		/* round-robin for now. The current core is taken into
		 * account separately. */
		acap_idx = n++ % n_acap;
		curr_cap = &curr_thd->cap[acap_idx];
		assert(curr_cap);
		if (unlikely(curr_cap->acap == 0)) { /* not used before. Set it up. */
			curr_cap->acap = ainv_intra_lookup(cos_spd_id(), acap_idx, fn);
			curr_cap->shared_page = ainv_intra_lookup_ring(cos_spd_id(), acap_idx, fn);
			assert(curr_cap->acap && curr_cap->shared_page);
			init_intra_shared_page(&curr_cap->shared_struct, curr_cap->shared_page);
		}
		shared_struct = &curr_cap->shared_struct;
		assert(shared_struct->ring);

		inv.data = data;
		inv.fn = fn;
		//inv.ret = 
		
		printc("core %ld pushing fn %d, data %d in the ring.\n",
		       cos_cpuid(), (int)inv.fn, (int)inv.data);
		/* Write to the ring buffer. Spin when buffer is full. */
		while (unlikely(!CK_RING_ENQUEUE_SPSC(intra_inv_ring, shared_struct->ring, &inv))) ;

		/* decide if need to send ipi. */
		if (SERVER_ACTIVE(shared_struct)) continue;
		assert(curr_cap->acap);
		cos_ainv_send(curr_cap->acap);
	}

	return 0;
err_nomem:
	printc("cos: thd %d couldn't allocate memory for ainv structures.\n", cos_get_thd_id());
	return -1;
}

int parallel_inv(void *fn, void *data, int iter) {
	struct intra_ainv_info *curr_thd;
	int i, ret;

	ret = ainv_parallel_start(fn, data, iter);
	assert(ret == 0);
	
	curr_thd = thd_intra_ainv[cos_get_thd_id()];
	assert(curr_thd);
	/* Working on the current core */
	for (i = 0; i < iter; i += curr_thd->n_acap + 1) { // sending to other cores
		exec_fn(fn, 1, (int *)&data);
	}

	// TODO: parallel_end. waiting or not. Need a symbol 

	return 0;
}

int ainv_parallel_end(void) {
	int curr_thd_id = cos_get_thd_id();
	struct intra_ainv_info *curr_thd = thd_intra_ainv[curr_thd_id];
	assert(curr_thd);

	cos_ainv_wait(curr_thd->wait_acap);

	printc("thd %d after parallel_end\n", cos_get_thd_id());

	return 0;
}

static inline int ainv_get_thd_num(void) {
	int curr_thd_id = cos_get_thd_id();
	struct intra_ainv_info *curr_thd = thd_intra_ainv[curr_thd_id];
	assert(curr_thd);

	return curr_thd->thd_num;
}

/* Currently only the main thread can get the number of
 * threads. Return 1 for children threads. */
static inline int ainv_get_num_thds(void) {
	int curr_thd_id = cos_get_thd_id();
	struct intra_ainv_info *curr_thd = thd_intra_ainv[curr_thd_id];

	assert(curr_thd);
	return curr_thd->n_acap + 1;
}

int cos_intra_ainv_handling(void) {
	struct __cos_ainv_srv_thd_intra curr_data;
	struct __cos_ainv_srv_thd_intra *curr = &curr_data;
	/* for worker thread, we access thread number from this structure. */
	struct intra_ainv_info thd_info;
	struct intra_ainv_info *info = &thd_info;
	thd_intra_ainv[cos_get_thd_id()] = info;

	int acap, i, parent_id, ret;
	int curr_thd_id = cos_get_thd_id();
	assert(curr);

	printc("upcall thread %d (core %ld) waiting in spd %d...\n", cos_get_thd_id(), cos_cpuid(), cos_spd_id());
	sched_block(cos_spd_id(), 0);
	printc("upcall thread %d (core %ld) up!\n", cos_get_thd_id(), cos_cpuid());
		
	curr->acap = acap_srv_lookup_intra(cos_spd_id());
	curr->shared_page = acap_srv_lookup_ring_intra(cos_spd_id());
	assert(curr->acap && curr->shared_page);
	acap = curr->acap;

	ret = acap_srv_lookup_thd_num(cos_spd_id());
	parent_id = ret >> 16;
	/* save those info on stack for efficiency. */
	info->thd_num = ret && 0xFFFF;
	info->parent = thd_intra_ainv[parent_id];
	info->n_acap = info->parent->n_acap;

	init_intra_shared_page(&curr->intra_shared_struct, curr->shared_page);

	printc("server %ld, upcall thd %d has acap %d.\n", 
	       cos_spd_id(), curr_thd_id, acap);

	struct intra_shared_struct *shared_struct = &curr->intra_shared_struct;
	CK_RING_INSTANCE(intra_inv_ring) *ring = shared_struct->ring;
	assert(ring);

	struct intra_inv_data inv;
	while (curr->stop == 0) {
		CLEAR_SERVER_ACTIVE(shared_struct); // clear active early to avoid race (and atomic instruction)
		if (CK_RING_DEQUEUE_SPSC(intra_inv_ring, ring, &inv) == false) {
			assert(info->parent);
			info->parent->finished++; // TODO atomic. inc and fetch!
			if (info->parent->finished == info->parent->n_acap)
				cos_ainv_send(info->parent->wakeup_acap);

			printc("thread %d waiting on acap %d\n", cos_get_thd_id(), acap);
			cos_ainv_wait(acap);
			printc("thread %d up from ainv_wait\n", cos_get_thd_id());
		} else {
			SET_SERVER_ACTIVE(shared_struct); /* setting us active */
			printc("core %ld: got inv for data %d, fn %d\n", cos_cpuid(), (int)inv.data, (int)inv.fn);
			if (unlikely(!inv.fn)) {
				printc("Server thread %d in comp %ld: receiving invalid fn %d\n",
				       cos_get_thd_id(), cos_spd_id(), (int)inv.fn);
			} else {
				//execute!
				exec_fn(inv.fn, 1, (int *)&inv.data);
				// and write to the return value.
			}
		}
	}

	return 0;
}

#endif /* !PARALLEL_INV_H */

