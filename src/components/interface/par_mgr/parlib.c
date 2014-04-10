#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>

#include <cos_synchronization.h>
#include <parlib.h>
#include "../interface/par_mgr/par_mgr.h"

struct par_thd_info *__par_thd_info[MAX_NUM_THREADS];  // TODO: replace with cvect 

int omp_get_thread_num() {
	/* The value is not valid when nested parallel presents. */
	return ainv_get_thd_num();
}

int omp_get_num_threads() {
	/* The value is not valid when nested parallel presents. */
	return ainv_get_num_thds();
}

int omp_get_max_threads() {
	return ainv_get_max_thds();
}

int omp_set_nested(int enable) {
	/* always enable for now. */
	return 0;
}

/* num_threads = 0 means decided by run-time library. 1 means no
 * parallelism. > 1 means max number of threads. */
int
GOMP_parallel_start (void *fn, void *data, unsigned int num_threads)
{
	int max_par;
	/* printc("core %ld, gomp start: fn %d, data %d, go parallel %u\n", */
	/*        cos_cpuid(), (int)fn, (int)data, num_threads); */
	if (unlikely(num_threads == 1)) return 0; /* means no parallelism. */

	if (likely(num_threads == 0)) max_par = NUM_CPU_COS;
	else                          max_par = num_threads;

	ainv_parallel_start(fn, data, max_par);

	return 0;
}

/* chunk_size is used for omp scheduling. Ignored here. */
void
GOMP_parallel_loop_static_start (void (*fn) (void *), void *data,
				 unsigned num_threads, long start, long end,
				 long incr, long chunk_size)
{
	int max_par;
	/* printc("core %ld, parallel for start: fn %d, data %d, go parallel %u\n", */
	/*        cos_cpuid(), (int)fn, (int)data, num_threads); */
	if (num_threads == 1) return; /* means no parallelism. */

	if (num_threads == 0)
		max_par = NUM_CPU_COS;
	else
		max_par = num_threads;
	ainv_parallel_loop_start(fn, data, max_par, start, end, incr);

	return;
}

// this needs to use functions in cos_synchronization.o
/* cos_lock_t omp_lock; */
/* #define LOCK()      do { if (lock_take(&omp_lock))    BUG(); } while(0); */
/* #define UNLOCK()    do { if (lock_release(&omp_lock)) BUG(); } while(0); */

void
GOMP_atomic_start(void)
{
	/* LOCK(); */
}

void
GOMP_atomic_end(void)
{
	/* UNLOCK(); */
}

int 
GOMP_loop_static_next (long *istart, long *iend)
{
//	if ()
	return 0;
}

void
GOMP_loop_end_nowait (void)
{
	/* nothing need to be done for us. */
	return;
}


int 
GOMP_parallel_end() {
	ainv_parallel_end();

	return 0;
}

int
cos_intra_ainv_handling(void)
{
	struct par_srv_thd_info curr_data;
	struct par_srv_thd_info *curr = &curr_data;
	/* for worker thread, we access thread number from this structure. */
	struct par_thd_info thd_info;
	struct intra_shared_struct *shared_struct;
	struct nested_par_info *barrier_info;
	struct __intra_inv_data inv = { .data = 0 };

	int acap, parent_id, nest_level, ret, barrier_acap;
	int thd_id = cos_get_thd_id();

	/* printc("upcall thread %d (core %ld) waiting in spd %ld...\n",  */
	/*        cos_get_thd_id(), cos_cpuid(), cos_spd_id()); */
	ret = sched_block(cos_spd_id(), 0);
	assert(ret == 0);
	/* printc("upcall thread %d (core %ld) up!\n", cos_get_thd_id(), cos_cpuid()); */

	curr->acap = par_srv_acap_lookup(cos_spd_id());
	acap = curr->acap;
	/* printc("server %ld, upcall thd %d has acap %d.\n",  */
	/*        cos_spd_id(), thd_id, acap); */

	curr->shared_page = par_srv_ring_lookup(cos_spd_id());
	init_intra_shared_page(&curr->intra_shared_struct, curr->shared_page);
	assert(curr->shared_page);
	shared_struct = &curr->intra_shared_struct;
	CK_RING_INSTANCE(intra_inv_ring) *ring = shared_struct->ring;
	assert(ring);

	ret = par_parent_lookup(cos_spd_id());
	if (ret == 0) {
		/* This means the current thread is for multicast IPI
		 * distribution. */
		cos_multicast_distribution(curr);
		return 0;
	}

	parent_id = ret >> 16;
	nest_level = ret & 0xFFFF;
	curr->parent = __par_thd_info[parent_id];
	assert(curr->parent);
	barrier_info = &curr->parent->nested_par[nest_level];
	assert(barrier_info);
	barrier_acap = barrier_info->wakeup_acap;

	__par_thd_info[thd_id] = &thd_info;
	thd_info.n_cpu = 0; /* means the current thread has no parallel yet. */
	/* save the thd num on stack for efficiency. */
	thd_info.thd_num = par_srv_thd_num_lookup(cos_spd_id());
	thd_info.orig_thd_num = thd_info.thd_num;
	thd_info.num_thds = curr->parent->n_cpu;
	thd_info.orig_num_thds = thd_info.num_thds;
	assert(thd_info.num_thds > 1);
	SET_SERVER_ACTIVE(shared_struct); /* setting us active */

	while (1) {
		if (acap > 0) {
			CLEAR_SERVER_ACTIVE(shared_struct); // clear active early to avoid race (and atomic instruction)
			/*
			 * If the ring buffer has no pending events for us to
			 * act on, then we should wait for the next event
			 * notification.
			 */
			while (CK_RING_DEQUEUE_SPSC(intra_inv_ring, ring, &inv) == false) {
				/* printc("thread %d waiting on acap %d\n", cos_get_thd_id(), acap); */
				ret = cos_areceive(acap); 
				assert(ret == 0);
				/* printc("thread %d up from areceive\n", cos_get_thd_id()); */
			}
			SET_SERVER_ACTIVE(shared_struct); /* setting us active */
		} else {
			while (CK_RING_DEQUEUE_SPSC(intra_inv_ring, ring, &inv) == false) ;
		}
		/* printc("core %ld, thd %d (thd num %d): got inv for data %d, fn %d\n", */
		/*        cos_cpuid(), cos_get_thd_id(), ainv_get_thd_num(), (int)inv.data, (int)inv.fn); */
		if (unlikely(!inv.fn)) {
			printc("Server thread %d in comp %ld: terminating.\n",
			       cos_get_thd_id(), cos_spd_id());
			__par_thd_info[thd_id] = NULL;
			break;
		}

		exec_fn(inv.fn, 1, (int *)&inv.data);
		
		ret = ck_pr_faa_32(&barrier_info->finished, 1); //fetch and incr
		/* printc("worker thd %d on core %ld got bar %d\n", */
		/*        cos_get_thd_id(), cos_cpuid(), ret); */

		/* barrier_acap < 0 means the master will be spinning
		 * for the synchronization. */
		if (barrier_acap > 0 && (ret == curr->parent->n_cpu - 1)) {
			/* printc("thd %d on core %ld sending to wakeup acap %d\n",  */
			/*        cos_get_thd_id(), cos_cpuid(), barrier_info->wakeup_acap); */
			cos_asend(barrier_acap);
		}
	}

	return 0;
}

/* Following functions are used for inter-component
 * async-invocation. */

struct ainv_info *thd_ainv[MAX_NUM_THREADS];        // TODO: replace with cvect 

int cos_thd_entry_static(u32_t idx)
{
	printc("No implementation yet.\n");
	BUG();

	return 0;
}

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
		acap = acap_cli_lookup(cos_spd_id(), cap_id, COS_STATIC_THD_ENTRY(0));
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

        /* printc("core %ld: sending to static cap %d, params %d, %d, %d, %d, acap %d\n", */
	/*        cos_cpuid(), cap_id, *params, *(params+1), *(params+2), *(params+3), acap); */

	struct inv_data inv;
	inv.cap = cap_id;
	memcpy(inv.params, params, sizeof(int) * 4); //figure out num of params?

	/* Write to the ring buffer. Spin when buffer is full. */
	while (unlikely(!CK_RING_ENQUEUE_SPSC(inv_ring, shared_struct->ring, &inv))) ;

	/* decide whether we should send ipi. return 0 if not. */
	if (SERVER_ACTIVE(shared_struct)) return 0;
	
	/* instead of returning acap and go through inv path, why
	 * don't we call asend here? We can get rid of the branch
	 * in the inv path (which is to detect async flag). */

	return acap;
err_nomem:
	return -1;
}
