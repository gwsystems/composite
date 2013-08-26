#ifndef PARALLEL_INV_H
#define PARALLEL_INV_H

#include <acap_mgr_intra.h>
#include <ck_pr.h>

/* Structure of the ring buffer page. */
struct intra_shared_struct {
	int *server_active;
	CK_RING_INSTANCE(intra_inv_ring) *ring;
};

/* Master thread structures. */
struct par_cap_info {
	int acap;
	void *shared_page;
	struct intra_shared_struct shared_struct;
};

struct nested_par_info {
	struct par_cap_info *cap; /* acaps for the "current" master thread. */
	int wait_acap, wakeup_acap;
	unsigned int finished CACHE_ALIGNED; // FIXME: ensure alignment???
};

struct par_thd_info {
	int thd_num;    /* thread number in the parallel team. 0 means
			 * master thread.*/
	int num_thds;   /* # of threads in the current team. */
	int orig_thd_num, orig_num_thds; /* Used when have nesting */
	int n_cpu;      /* 0 means never created */
	int n_acap;     /* 0 means sequentially. Equals n_cpu - 1 */

	int nest_level; /* nesting level of the current thread. */
	struct nested_par_info nested_par[MAX_NESTED_PAR_LEVEL]; 
};

extern struct par_thd_info *__par_thd_info[MAX_NUM_THREADS];

/* Worker thread structures. */
struct par_srv_thd_info {
	int acap;
	void *shared_page;
	struct par_thd_info *parent;

	struct intra_shared_struct intra_shared_struct;
} CACHE_ALIGNED;

static inline void 
init_intra_shared_page(struct intra_shared_struct *curr, void *page) {
	/*The ring starts from the second cache line of the
	 * page. (First cache line is used for the server thread
	 * active flag)*/
	curr->server_active = (int *)page;
	/* ring initialized by acap mgr. see comments in
	 * alloc_share_page in acap_mgr. */
	curr->ring = (CK_RING_INSTANCE(intra_inv_ring) *) (page + CACHE_LINE);
}

/* Utility functions below. */

static inline int 
parallel_create(void *fn, int max_par) // fn is not used for now.
{
	int curr_thd_id = cos_get_thd_id();
	struct par_thd_info *curr_thd = __par_thd_info[curr_thd_id];
	struct par_cap_info *curr_cap;
	struct nested_par_info *par_team;
	
	if (unlikely(curr_thd == NULL)) {
		/* only happen for a "root" thread. All child threads
		 * allocate the curr_thd structure on their stack when
		 * they are created. */
		__par_thd_info[curr_thd_id] = malloc(sizeof(struct par_thd_info));
		curr_thd = __par_thd_info[curr_thd_id];
		if (unlikely(curr_thd == NULL)) goto err_nomem;
		curr_thd->n_cpu = 0;
		curr_thd->thd_num = 0;
		curr_thd->orig_thd_num = 0;
		curr_thd->orig_num_thds = 1;
	}

	if (unlikely(curr_thd->n_cpu == 0)) {
		int ret;
		/* This happens when a thread tries to do parallel for
		 * the first time. */
		curr_thd->nest_level = 0;

		ret = par_create(cos_spd_id(), max_par - 1);
		curr_thd->n_acap = ret & 0xFFFF;
		curr_thd->n_cpu = ret >> 16;
		/* printc("thd %d got # of acap: %d\n", curr_thd_id, curr_thd->n_acap); */
		if (unlikely(curr_thd->n_acap < 0)) {
			printc("cos: Intra-comp ainv creation failed.\n");
			return -1;
		}
		curr_thd->num_thds = curr_thd->n_cpu;
	}
	par_team = &curr_thd->nested_par[curr_thd->nest_level];

	if (unlikely(par_team->cap == NULL && curr_thd->n_acap > 1)) {
		int i, n_acap = curr_thd->n_acap;
		/* This happens when a thread tries to do parallel on
		 * the current nesting level for the first time. */
		par_team->cap = malloc(sizeof(struct par_thd_info) * curr_thd->n_acap);
		if (unlikely(par_team->cap == NULL)) goto err_nomem;
	}
	assert(curr_thd && par_team->cap);

	if (unlikely(par_team->wait_acap == 0 && curr_thd->n_acap > 0)) {
		int ret = par_acap_get_barrier(cos_spd_id(), curr_thd->nest_level);
		par_team->wait_acap = ret >> 16;
		par_team->wakeup_acap = ret & 0xFFFF;
		assert (par_team->wakeup_acap && par_team->wait_acap);
		/* printc("main thd %d has wait acap %d and wakeup acap %d\n",  */
		/*        curr_thd_id, par_team->wait_acap, par_team->wakeup_acap); */
	}

	/* The thread number and # of threads are updated here for
	 * nested parallelism. Will restore them in parallel_end. */
	curr_thd->thd_num = 0;
	curr_thd->num_thds = curr_thd->n_cpu;

	par_team->finished = 0;
	curr_thd->nest_level++;
	
	return curr_thd->n_acap;
err_nomem:
	printc("cos: thd %d couldn't allocate memory for ainv structures.\n", cos_get_thd_id());
	return -1;
}

static inline int 
parallel_send(void *fn, void *data)
{
	int i, n_acap, curr_thd_id = cos_get_thd_id();
	struct par_thd_info *curr_thd = __par_thd_info[curr_thd_id];
	struct nested_par_info *par_team;
	struct par_cap_info *curr_cap;
	struct intra_shared_struct *shared_struct;
	struct intra_inv_data inv;
	unsigned long long s, e;

	int curr_nest = curr_thd->nest_level - 1;;
	assert(curr_thd && curr_thd->nest_level > 0);
	n_acap = curr_thd->n_acap;
	par_team = &curr_thd->nested_par[curr_nest];

	inv.data = data;
	inv.fn = fn;

	if (unlikely(par_team->cap[0].acap == 0)) {
		/* Not used before for the current nesting level. Set
		 * it up. */
		for (i = 0; i < n_acap; i++) {
			curr_cap = &par_team->cap[i];
			assert(curr_cap->acap == 0);
			curr_cap->acap = par_acap_lookup(cos_spd_id(), i, curr_nest);
			curr_cap->shared_page = par_ring_lookup(cos_spd_id(), i, curr_nest);

			assert(curr_cap->acap && curr_cap->shared_page);
			init_intra_shared_page(&curr_cap->shared_struct, curr_cap->shared_page);
		}
	}

	for (i = 0; i < n_acap; i++) { // sending to other cores
		curr_cap = &par_team->cap[i];
		assert(curr_cap && curr_cap->acap);
		shared_struct = &curr_cap->shared_struct;
		assert(shared_struct && shared_struct->ring);
		
		/* printc("thd %d on core %ld pushing fn %d, data %d in the ring.\n", */
		/*        cos_get_thd_id(), cos_cpuid(), (int)inv.fn, (int)inv.data); */
		/* Write to the ring buffer. Spin when buffer is full. */
		while (unlikely(!CK_RING_ENQUEUE_SPSC(intra_inv_ring, shared_struct->ring, &inv))) 
		{
			if (unlikely(s == 0)) rdtscll(s); 
			rdtscll(e);
			/* detect unusual delay */
			if (e - s > 1 << 30) {
				printc("parallel execution: comp %ld pushing into ring buffer has abnormal delay (%llu cycles).\n", 
				       cos_spd_id(), e - s);
				s = e;
			}
		}

		/* decide if need to send ipi. */
		if (SERVER_ACTIVE(shared_struct)) continue;
		assert(curr_cap->acap);
		cos_ainv_send(curr_cap->acap);
	}

	return 0;
}

static inline int 
ainv_parallel_start(void *fn, void *data, int max_par) 
{
	int n_acap;
	/* printc("thd %d parallel start!\n", cos_get_thd_id()); */
	n_acap = parallel_create(fn, max_par);

	if (n_acap > 0) parallel_send(fn, data);

	/* printc("thd %d parallel start done!\n", cos_get_thd_id()); */

	return 0;
}

int 
ainv_parallel_end(void) 
{
	int curr_thd_id = cos_get_thd_id(), nest, wait_acap, ret;
	struct par_thd_info *curr_thd = __par_thd_info[curr_thd_id];
	struct nested_par_info *par_team;
	/* printc("thd %d parallel_end!\n", cos_get_thd_id()); */

	assert(curr_thd);
	nest = curr_thd->nest_level - 1;
	assert(nest >= 0);
	par_team = &curr_thd->nested_par[nest];

	if (curr_thd->n_acap > 0) {
		ret = ck_pr_faa_32(&par_team->finished, 1); //fetch and incr
		/* If the master thread is the last one to finish,
		 * then no need to wait. */
		/* printc("core %ld thd %d got bar cnt %d\n", */
		/*        cos_cpuid(), cos_get_thd_id(), ret); */

		if (ret < curr_thd->n_cpu - 1) {
			wait_acap = par_team->wait_acap;
			assert(wait_acap > 0);
			/* printc("core %ld thd %d waiting on acap %d\n", */
			/*        cos_cpuid(), cos_get_thd_id(), wait_acap); */
			ret = cos_ainv_wait(wait_acap);
			assert(ret == 0);
		}
	}
	curr_thd->nest_level--;
	assert(curr_thd->nest_level >= 0);

	if (curr_thd->nest_level == 0) {
		/* Restore the value if we finished nested parallel
		 * sections. */
		curr_thd->thd_num = curr_thd->orig_thd_num;
		curr_thd->num_thds = curr_thd->orig_num_thds;
	}

	return 0;
}

/* Not needed yet. */
static inline int
parallel_loop_init(long start, long end, long incr) {
	return 0;
}

/* Not needed yet. */
static inline int 
ainv_parallel_loop_start(void (*fn) (void *), void *data, unsigned num_threads, 
			 long start, long end, long incr)
{
	int n_acap;
	n_acap = parallel_create(fn, num_threads);
	parallel_loop_init(start, end, incr);
	if (n_acap > 0) 
		parallel_send(fn, data);

	return 0;
}

int 
parallel_inv(void *fn, void *data, int max_par) 
{
	struct par_thd_info *curr_thd;
	int i, ret;

	ret = ainv_parallel_start(fn, data, max_par);
	assert(ret == 0);
	
	curr_thd = __par_thd_info[cos_get_thd_id()];
	assert(curr_thd);
	/* Working on the current core */
	exec_fn(fn, 1, (int *)&data);

	// TODO: waiting or not. Need a symbol 
	ainv_parallel_end();

	return 0;
}

static inline int 
ainv_get_thd_num(void) 
{
	int curr_thd_id = cos_get_thd_id();
	struct par_thd_info *curr_thd = __par_thd_info[curr_thd_id];
	if (unlikely(!curr_thd)) return 0;
	/* printc("core %ld, thread %d got thd num %d\n", cos_cpuid(), cos_get_thd_id(), curr_thd->thd_num); */

	return curr_thd->thd_num;
}

static inline int 
ainv_get_num_thds(void) 
{
	/* FIXME: when the max level of parallelism is less than
	 * n_cpu, this is not correct. */
	int curr_thd_id = cos_get_thd_id();
	struct par_thd_info *curr_thd = __par_thd_info[curr_thd_id];
	if (unlikely(!curr_thd)) return 1;
	/* printc("core %ld got # of thds %d\n", cos_cpuid(), curr_thd->num_thds); */

	return curr_thd->num_thds;
}

static inline int 
ainv_get_max_thds(void)
{
	int curr_thd_id = cos_get_thd_id();
	struct par_thd_info *curr_thd = __par_thd_info[curr_thd_id];
	assert(curr_thd);

	return curr_thd->n_cpu;
}

static inline int 
multicast_send(struct par_cap_info acaps[], int n_acap, struct intra_inv_data *orig_inv)
{
	int i;
	struct par_cap_info *curr_cap;
	struct intra_shared_struct *shared_struct;
	unsigned long long s, e;
	struct intra_inv_data inv = *orig_inv;

	for (i = n_acap - 1; i >= 0; i--) { // sending to other cores
		curr_cap = &acaps[i];
		assert(curr_cap && curr_cap->acap);
		shared_struct = &curr_cap->shared_struct;
		assert(shared_struct && shared_struct->ring);

		/* printc("thd %d on core %ld pushing fn %d, data %d in the ring.\n", */
		/*        cos_get_thd_id(), cos_cpuid(), (int)inv.fn, (int)inv.data); */
		/* Write to the ring buffer. Spin when buffer is full. */

		while (unlikely(!CK_RING_ENQUEUE_SPSC(intra_inv_ring, shared_struct->ring, &inv)))
		{
			if (unlikely(s == 0)) rdtscll(s);
			rdtscll(e);
			/* detect unusual delay */
			if (e - s > 1 << 30) {
				printc("parallel execution: comp %ld pushing into ring buffer has abnormal delay (%llu cycles).\n",
				       cos_spd_id(), e - s);
				s = e;
			}
		}

		/* decide if need to send ipi. */
		if (SERVER_ACTIVE(shared_struct)) continue;
		assert(curr_cap->acap);
		cos_ainv_send(curr_cap->acap);
	}

	return 0;
}

static inline int 
cos_multicast_distribution(struct par_srv_thd_info *curr)
{
	int i, ret, acap = curr->acap, n_acap = 0;
	struct par_cap_info forward_acaps[NUM_CORE_PER_SOCKET], *curr_cap;
	struct intra_shared_struct *shared_struct = &curr->intra_shared_struct;
	CK_RING_INSTANCE(intra_inv_ring) *ring = shared_struct->ring;
	struct intra_inv_data inv;
	assert(ring);

	while ((ret = par_acap_lookup(cos_spd_id(), n_acap, 0)) > 0) {
		curr_cap = &forward_acaps[n_acap];
		curr_cap->acap = ret;
		curr_cap->shared_page = par_ring_lookup(cos_spd_id(), n_acap, 0);
		assert(curr_cap->shared_page);
		init_intra_shared_page(&curr_cap->shared_struct, curr_cap->shared_page);

		/* printc("dist t %d got acap %d\n", cos_get_thd_id(), forward_acaps[n_acap]); */
		n_acap++;
	}

	int cnt = 0;
	while (1) {
		CLEAR_SERVER_ACTIVE(shared_struct); // clear active early to avoid race (and atomic instruction)
		/* 
		 * If the ring buffer has no pending events for us to
		 * act on, then we should wait for the next event
		 * notification.
		 */
		while (CK_RING_DEQUEUE_SPSC(intra_inv_ring, ring, &inv) == false) {
			/* printc("thread %d waiting on acap %d\n", cos_get_thd_id(), acap); */
			ret = cos_ainv_wait(acap);
			assert(ret == 0);
			/* printc("thread %d up from ainv_wait\n", cos_get_thd_id()); */
		} 

		SET_SERVER_ACTIVE(shared_struct); /* setting us active */
		/* printc("core %ld, thd %d: got inv for data %d, fn %d\n",  */
		/*        cos_cpuid(), cos_get_thd_id(), (int)inv.data, (int)inv.fn); */
		if (unlikely(!inv.fn)) {
			printc("Server thread %d in comp %ld: receiving invalid fn %d\n",
			       cos_get_thd_id(), cos_spd_id(), (int)inv.fn);
			assert(0);
			/* TODO: add code for thread termination here */
		} 
		
		multicast_send(forward_acaps, n_acap, &inv);
	}

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
	struct intra_inv_data inv;

	int acap, i, parent_id, nest_level, ret;
	int thd_id = cos_get_thd_id();

	/* printc("upcall thread %d (core %ld) waiting in spd %ld...\n",  */
	/*        cos_get_thd_id(), cos_cpuid(), cos_spd_id()); */
	ret = sched_block(cos_spd_id(), 0);
	assert(ret == 0);
	/* printc("upcall thread %d (core %ld) up!\n", cos_get_thd_id(), cos_cpuid()); */

	curr->acap = par_srv_acap_lookup(cos_spd_id());
	acap = curr->acap;
	assert(acap);
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

	__par_thd_info[thd_id] = &thd_info;
	thd_info.n_cpu = 0; /* means the current thread has no parallel yet. */
	/* save the thd num on stack for efficiency. */
	thd_info.thd_num = par_srv_thd_num_lookup(cos_spd_id());
	thd_info.orig_thd_num = thd_info.thd_num;
	thd_info.num_thds = curr->parent->n_cpu;
	thd_info.orig_num_thds = thd_info.num_thds;
	assert(thd_info.num_thds > 1);

	while (1) {
		CLEAR_SERVER_ACTIVE(shared_struct); // clear active early to avoid race (and atomic instruction)
		/* 
		 * If the ring buffer has no pending events for us to
		 * act on, then we should wait for the next event
		 * notification.
		 */
		while (CK_RING_DEQUEUE_SPSC(intra_inv_ring, ring, &inv) == false) {
			/* printc("thread %d waiting on acap %d\n", cos_get_thd_id(), acap); */
			ret = cos_ainv_wait(acap);
			assert(ret == 0);
			/* printc("thread %d up from ainv_wait\n", cos_get_thd_id()); */
		} 

		SET_SERVER_ACTIVE(shared_struct); /* setting us active */
		/* printc("core %ld, thd %d (thd num %d): got inv for data %d, fn %d\n", */
		/*        cos_cpuid(), cos_get_thd_id(), ainv_get_thd_num(), (int)inv.data, (int)inv.fn); */
		if (unlikely(!inv.fn)) {
			printc("Server thread %d in comp %ld: receiving invalid fn %d\n",
			       cos_get_thd_id(), cos_spd_id(), (int)inv.fn);
			assert(0);
			/* TODO: add code for thread termination here */
		} 

		exec_fn(inv.fn, 1, (int *)&inv.data);
		
		ret = ck_pr_faa_32(&barrier_info->finished, 1); //fetch and incr
		/* printc("worker thd %d on core %ld got bar %d\n", */
		/*        cos_get_thd_id(), cos_cpuid(), ret); */

		if (ret == curr->parent->n_cpu - 1) {
			/* printc("thd %d on core %ld sending to wakeup acap %d\n",  */
			/*        cos_get_thd_id(), cos_cpuid(), barrier_info->wakeup_acap); */
			cos_ainv_send(barrier_info->wakeup_acap);
		}
	}

	return 0;
}

#endif /* PARALLEL_INV_H */

