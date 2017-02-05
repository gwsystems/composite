/**
 * Copyright 2013 by The George Washington University.  All rights
 * reserved. Redistribution of this file is permitted under the GNU
 * General Public License v2.
 *
 * Author: Qi Wang, interwq@gwu.edu, 2013
 */

#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cos_alloc.h>
#include <valloc.h>
#include <mem_mgr_large.h>

#include <par_mgr.h>
#include <bitmap.h>

#define SPIN

#ifdef SPIN
#define PAR_CREATION_SPIN
#define PAR_BARRIER_SPIN
#endif

/* Use unicast for spin. */
/* and multicast for IPI. */
#ifdef SPIN
#undef MULTICAST
#else
#define MULTICAST
#endif

/* Assumption: master thread should be on the first core in the array. */
int assign[NUM_CPU_COS + 10] = {0, 1, -1};
/* int assign[50] = {0, 4, 8, 12, 16, 20, 24, 28, 32, 36, */
/* 		  1, 5, 9, 13, 17, 21, 25, 29, 33, 37, */
/* 		  2, 6, 10, 14, 18, 22, 26, 30, 34, 38, */
/* 		  3, 7, 11, 15, 19, 23, 27, 31, 35, -1}; */

struct srv_thd_info { 
	/* information of handling thread */
	int cli_spd_id;
	int srv_spd_id;
	int srv_acap; /* the server side acap that we do areceive on. */
	vaddr_t srv_ring;
	vaddr_t mgr_ring;
	int cli_thd;
	int cli_cap_id;
	
	/* following are required by intra component parallel
	 * execution (and openmp). */
	int thd_num; /* thread number as in omp */
	int parent; /* parent thread id. */
	int nesting_level; /* the nesting level of parallel section */
} srv_thd_info[MAX_NUM_THREADS];

struct static_cap {
	int srv_comp;
	void *srv_fn;
};

struct per_cap_thd_info {
	int acap;
	vaddr_t cap_ring;
	int cap_srv_thd;
};

struct cli_thd_info {
	/* mapping between static cap and other info (acap, ring
	 * buffer and server thread) for a thread within a specific
	 * component. */
	int ncaps;
	struct per_cap_thd_info *cap_info;
};

struct comp_info {
	int comp_id;
	int ncaps; //number of static caps
	struct static_cap *cap;
	/* acap for each thread in this component. */
	struct cli_thd_info *cli_thd_info[MAX_NUM_THREADS];
} comp_info[MAX_NUM_SPDS];


static int create_thd_curr_prio(int core, int spdid, int thd_init_idx) 
{
	union sched_param sp, sp1;
	int ret;
	sp.c.type = SCHEDP_RPRIO; // same priority as the parent
	sp.c.value = 0;

	sp1.c.type = SCHEDP_CORE_ID;
	sp1.c.value = core;
	ret = cos_thd_create_remote(spdid, thd_init_idx, sp.v, sp1.v, 0);

	if (!ret) BUG();

	return ret;
}

static int create_thd_prio(int core, int prio, int spdid, int thd_init_idx) {
	union sched_param sp, sp1;
	int ret;

	sp.c.type = SCHEDP_PRIO;
	sp.c.value = prio;

	sp1.c.type = SCHEDP_CORE_ID;
	sp1.c.value = core;
	ret = cos_thd_create_remote(spdid, thd_init_idx, sp.v, sp1.v, 0);

	if (!ret) BUG();

	return ret;
}

static int shared_page_setup(int thd_id) 
{
	// thd_id is the upcall thread on the server side.

	struct srv_thd_info *thd;
	int cspd, sspd;
	vaddr_t ring_mgr, ring_cli, ring_srv;

	assert(thd_id);

	thd = &srv_thd_info[thd_id];
	cspd = thd->cli_spd_id;
	sspd = thd->srv_spd_id;
	
	if (!cspd || !sspd) goto err;

	ring_mgr = (vaddr_t)alloc_page();
	if (!ring_mgr) {
		printc("par_mgr: alloc ring buffer failed in mgr %ld.\n", cos_spd_id());
		goto err;
	}

	srv_thd_info[thd_id].mgr_ring = ring_mgr;

	ring_cli = (vaddr_t)valloc_alloc(cos_spd_id(), cspd, 1);
	if (unlikely(!ring_cli)) {
		printc("par_mgr: vaddr alloc failed in client comp %d.\n", cspd);
		goto err_cli;
	}
	if (unlikely(ring_cli != mman_alias_page(cos_spd_id(), ring_mgr, cspd, ring_cli, MAPPING_RW))) {
		printc("par_mgr: alias to client %d failed.\n", cspd);
		goto err_cli_alias;
	}
	comp_info[cspd].cli_thd_info[cos_get_thd_id()]->cap_info[thd->cli_cap_id].cap_ring = ring_cli;
	
	ring_srv = (vaddr_t)valloc_alloc(cos_spd_id(), sspd, 1);
	if (unlikely(!ring_srv)) {
		goto err_srv;
		printc("par_mgr: vaddr alloc failed in server comp  %d.\n", sspd);
	}
	if (unlikely(ring_srv != mman_alias_page(cos_spd_id(), ring_mgr, sspd, ring_srv, MAPPING_RW))) {
		printc("par_mgr: alias to server %d failed.\n", sspd);
		goto err_srv_alias;
	}
	srv_thd_info[thd_id].srv_ring = ring_srv;

	/* Initialize the ring buffer. Passing NULL because we use
	 * continuous ring (struct + data region). The ring starts
	 * from the second cache line of the page. (First cache line
	 * is used for the server thread active flag) */
	CK_RING_INIT(inv_ring, (CK_RING_INSTANCE(inv_ring) *)((void *)ring_mgr + CACHE_LINE), NULL, 
		     leqpow2((PAGE_SIZE - CACHE_LINE - sizeof(CK_RING_INSTANCE(inv_ring))) / 2 / sizeof(struct inv_data)));
	
	return 0;

err_srv_alias:
	valloc_free(cos_spd_id(), sspd, (void *)ring_srv, 1);
err_srv:
	mman_revoke_page(cos_spd_id(), ring_mgr, 0); 
err_cli_alias:
	valloc_free(cos_spd_id(), cspd, (void *)ring_cli, 1);
err_cli:
	free_page((void *)ring_mgr);
err:	
	return -1;
}

/* External functions below. */

static inline int __srv_lookup(int spdid) 
{
	int acap, thd_id = cos_get_thd_id();
	struct srv_thd_info *thd;
	thd = &srv_thd_info[thd_id];
	
 	if (unlikely(thd->srv_spd_id != spdid)) goto err_spd;
	
	acap = thd->srv_acap;
	assert(acap);
	
	return acap;
err_spd:
	printc("par_mgr: upcall thread calling lookup from wrong component %d.\n", spdid);
	return -1;
}

/* Return server side acap id for the server upcall thread. */
int acap_srv_lookup(int spdid) 
{
	return __srv_lookup(spdid);
}

int par_srv_acap_lookup(int spdid) 
{
	return __srv_lookup(spdid);
}

int acap_srv_ncaps(int spdid) 
{
	int ncaps, thd_id = cos_get_thd_id();
	struct srv_thd_info *thd;

	thd = &srv_thd_info[thd_id];
	assert(thd->srv_acap);

	if (unlikely(thd->srv_spd_id != spdid)) goto err_spd;
	
	ncaps = comp_info[thd->cli_spd_id].ncaps;
	assert(ncaps);
	
	return ncaps;
err_spd:
	printc("par_mgr: upcall thread calling lookup from wrong component %d.\n", spdid);
	return -1;
}

/* Called by the upcall thread in the server component. */
void *acap_srv_fn_mapping(int spdid, int cap) 
{
	int thd_id = cos_get_thd_id();
	struct srv_thd_info *thd = &srv_thd_info[thd_id];

	if (spdid != thd->srv_spd_id) return 0;

	struct comp_info *ci = &comp_info[thd->cli_spd_id];
	assert(ci);
	if (ci->ncaps == 0 || cap > ci->ncaps || ci->cap[cap].srv_comp != spdid) return 0;

	/* printc("returning srv fn %x for cap %d\n",  ci->cap[cap].srv_fn, cap); */
	return ci->cap[cap].srv_fn;
}

static inline void *
__srv_ring_lookup(int spdid)
{
	int thd_id = cos_get_thd_id();
	struct srv_thd_info *thd = &srv_thd_info[thd_id];

	if (spdid != thd->srv_spd_id) return 0;

	return (void *)thd->srv_ring;
}

void *
par_srv_ring_lookup(int spdid)
{
	return __srv_ring_lookup(spdid);
}

void *
acap_srv_lookup_ring(int spdid) 
{
	return __srv_ring_lookup(spdid);
}

void *
acap_cli_lookup_ring(int spdid, int cap_id) 
{
	struct comp_info *ci = &comp_info[spdid];
	int thd_id = cos_get_thd_id();
	assert(ci);

	if (unlikely(cap_id > ci->ncaps)) return 0;
	assert(ci->cap);

	if (ci->cli_thd_info == NULL) return 0;

	return (void *)ci->cli_thd_info[thd_id]->cap_info[cap_id].cap_ring;
}

/* Return the acap (value) of the current thread (for the static cap_id) on
 * the client side. Major setup done in this function. */
int acap_cli_lookup(int spdid, int cap_id, int thd_init_idx) 
{
	int srv_thd_id, acap_id, acap_v, srv_acap, cspd, sspd, ret;
	struct srv_thd_info *srv_thd;
	struct cli_thd_info *cli_thd_info;
	struct comp_info *ci = &comp_info[spdid];
	int thd_id = cos_get_thd_id();

	if (unlikely(cap_id > ci->ncaps)) goto err_cap;
	assert(ci->cap);

	cspd = spdid;
	sspd = ci->cap[cap_id].srv_comp;

	cli_thd_info = ci->cli_thd_info[thd_id];

	if (cli_thd_info == NULL) {
		ci->cli_thd_info[thd_id] = malloc(sizeof(struct cli_thd_info));
		cli_thd_info = ci->cli_thd_info[thd_id];
		cli_thd_info->ncaps = ci->ncaps;
		cli_thd_info->cap_info = malloc(sizeof(struct per_cap_thd_info) * ci->ncaps);
	}
	if (unlikely(cli_thd_info == NULL || cli_thd_info->cap_info == NULL)) {
		printc("acap mgr %ld: cannot allocate memory for thd_info structure.\n", cos_spd_id());
		goto err;
	}

	if (cli_thd_info->cap_info[cap_id].acap > 0) //already exist?
		return cli_thd_info->cap_info[cap_id].acap;

	/* TODO: lookup some decision table (set by the policy) to
	 * decide whether create a acap for the current thread and the
	 * destination cpu */
	//also, if an acap can be used for multiple s_caps, reflect it here.
//	lookup(thd_id, spdid, cap_id);
// and if we want this thread use static cap, return 0
	int cpu = 1;

	/* create server thd. */
	srv_thd_id = create_thd_curr_prio(cpu, sspd, thd_init_idx);
	/* printc("Created handling thread %d on cpu %d\n", srv_thd_id, cpu); */
	srv_thd = &srv_thd_info[srv_thd_id];
	srv_thd->cli_spd_id = cspd;
	srv_thd->srv_spd_id = sspd;

	srv_thd->cli_thd = thd_id;
	srv_thd->cli_cap_id = cap_id;
	cli_thd_info->cap_info[cap_id].cap_srv_thd = srv_thd_id;

	/* create acap between cspd and sspd */

	ret = cos_async_cap_cntl(COS_ACAP_CREATE, cspd, sspd, srv_thd_id);
	acap_id = ret >> 16;
	srv_acap = ret & 0xFFFF;
	if (acap_id <= 0) { 
		printc("err: async cap creation failed.");
		goto err; 
	}
	acap_v = acap_id | COS_ASYNC_CAP_FLAG;
	cli_thd_info->cap_info[cap_id].acap = acap_v; /* client side acap */

	if (srv_acap <= 0) {
		printc("Server acap allocation failed for thread %d.\n", srv_thd_id);
		goto err;
	}
	srv_thd->srv_acap = srv_acap;

	ret = shared_page_setup(srv_thd_id);
	if (ret < 0) {
		printc("par_mgr: ring buffer allocation error!\n");
		goto err;
	}

	if (sched_wakeup(cos_spd_id(), srv_thd_id)) BUG();

	/* printc("par_mgr returning acap %d (%d), thd %d\n",  */
	/*        acap_v, acap_v & ~COS_ASYNC_CAP_FLAG, cos_get_thd_id()); */

	return acap_v;
err_cap:
	printc("par_mgr: thread %d calling lookup for non-existing cap %d in component %d.\n",
	       thd_id, cap_id, spdid);
err:
	return 0;
}

/* intra-comp parallel support below. */

struct nested_par_info {
	struct per_cap_thd_info *cap_info;
	int wait_acap, wakeup_acap;
};

struct intra_comp {
	int thdid;
	int spdid;
	int n_cpu, n_acap;
	int inter_socket;  /* # of inter-socket acaps */
	int dist_thd_idx;   /* > 0 if current is a distribution thd */
	int *cpus;
	struct nested_par_info nested_par[MAX_OMP_NESTED_PAR_LEVEL]; 
};

struct thd_intra_comp {
	struct intra_comp *comp[MAX_NUM_SPDS];
};

struct thd_intra_comp *thd_intra_comp[MAX_NUM_THREADS];

static inline int 
intra_shared_page_setup(int thd_id, struct per_cap_thd_info *cap_info) 
{
	// thd_id is the server upcall thread
	struct srv_thd_info *thd;
	int cspd, sspd;
	vaddr_t ring_mgr, ring_cli, ring_srv;

	assert(thd_id);

	thd = &srv_thd_info[thd_id];
	cspd = thd->cli_spd_id;
	sspd = thd->srv_spd_id;
	assert(cspd == sspd && cspd < MAX_NUM_SPDS);
	if (!cspd || !sspd) goto err;

	ring_mgr = (vaddr_t)alloc_page();
	if (!ring_mgr) {
		printc("par_mgr: alloc ring buffer failed in mgr %ld.\n", cos_spd_id());
		goto err;
	}

	srv_thd_info[thd_id].mgr_ring = ring_mgr;

	ring_cli = (vaddr_t)valloc_alloc(cos_spd_id(), cspd, 1);
	if (unlikely(!ring_cli)) {
		printc("par_mgr: vaddr alloc failed in client comp %d.\n", cspd);
		goto err_cli;
	}
	if (unlikely(ring_cli != mman_alias_page(cos_spd_id(), ring_mgr, cspd, ring_cli, MAPPING_RW))) {
		printc("par_mgr: alias to client %d failed.\n", cspd);
		goto err_cli_alias;
	}
	cap_info->cap_ring = ring_cli;
	
	ring_srv = ring_cli;
	srv_thd_info[thd_id].srv_ring = ring_srv;

	/* Initialize the ring buffer. Passing NULL because we use
	 * continuous ring (struct + data region). The ring starts
	 * from the second cache line of the page. (First cache line
	 * is used for the server thread active flag) */

        //intra-comp do not need return region in the ring

	CK_RING_INIT(intra_inv_ring, (CK_RING_INSTANCE(intra_inv_ring) *)((void *)ring_mgr + CACHE_LINE), NULL, 
		     leqpow2((PAGE_SIZE - CACHE_LINE - sizeof(CK_RING_INSTANCE(intra_inv_ring))) / sizeof(struct __intra_inv_data))); 
	
	return 0;

err_cli_alias:
	valloc_free(cos_spd_id(), cspd, (void *)ring_cli, 1);
err_cli:
	free_page((void *)ring_mgr);
err:	
	return -1;
}

static inline int 
assign_unicast(struct intra_comp *thd_comp)
{
	int i, j, curr_socket, assigned = 0, n_acap = thd_comp->n_cpu - 1;
	thd_comp->n_acap = n_acap;

	if (n_acap == 0) return 0;

	thd_comp->cpus = malloc(sizeof(int) * n_acap);
	if (unlikely(thd_comp->cpus == NULL)) return -1;

	for (i = 0; i < thd_comp->n_cpu; i++) {
		if (assign[i] == cos_cpuid()) continue;
		thd_comp->cpus[assigned++] = assign[i];
	}
	assert(assigned == n_acap);

	thd_comp->inter_socket = 0;
	thd_comp->dist_thd_idx = 0;

	return 0;
}

/* static inline int  */
/* assign_unicast_bestcase(struct intra_comp *thd_comp) */
/* { */
/* 	int i, j, curr_socket, assigned = 0, n_acap = thd_comp->n_cpu - 1; */
/* 	thd_comp->n_acap = n_acap; */

/* 	if (n_acap == 0) return 0; */

/* 	thd_comp->cpus = malloc(sizeof(int) * n_acap); */
/* 	if (unlikely(thd_comp->cpus == NULL)) return -1; */

/* 	//TODO: ask policy! */
/* 	curr_socket = cos_cpuid() % NUM_CPU_SOCKETS; //get current socket */
/* 	for (j = curr_socket; j < NUM_CPU_COS; j += NUM_CPU_SOCKETS) {// assign curr socket first */
/* 		if (j == cos_cpuid()) continue; */
/* 		/\* printc("core %ld thd %d: got cpu %d\n", *\/ */
/* 		/\*        cos_cpuid(), cos_get_thd_id(), i); *\/ */
/* 		thd_comp->cpus[assigned++] = j; */
/* 		if (assigned == n_acap) break; */
/* 	} */
/* 	for (i = 0; i < NUM_CPU_SOCKETS; i++) { // other sockets */
/* 		if (assigned == n_acap) break; */
/* 		if (i == curr_socket) continue; */
/* 		for (j = i; j < NUM_CPU_COS; j += NUM_CPU_SOCKETS) { */
/* 			if (j == cos_cpuid()) continue; */
/* 			/\* printc("core %ld thd %d: got cpu %d\n", *\/ */
/* 			/\*        cos_cpuid(), cos_get_thd_id(), i); *\/ */
/* 			thd_comp->cpus[assigned++] = j; */
/* 			if (assigned == n_acap) break; */
/* 		} */
/* 	} */
/* 	assert(assigned == n_acap); */

/* 	thd_comp->inter_socket = 0; */
/* 	thd_comp->dist_thd_idx = 0; */

/* 	return 0; */
/* } */

/* An assumption here, the core ids are assigned in a way that iterates
 * sockets first. */
static inline int 
assign_multicast(struct intra_comp *thd_comp)
{
	int i, curr_socket, inter_socket, n_acap, cpu_idx, n_cpu = thd_comp->n_cpu;
	/* for each socket, assign the distribution thread to the
	 * first cpu in that socket appeared in the list. */
	int dist_cpu[NUM_CPU_SOCKETS], local_cores = 0;
	
	for (i = 0; i < NUM_CPU_SOCKETS; i++) dist_cpu[i] = -1;
	
	curr_socket = cos_cpuid() % NUM_CPU_SOCKETS; 
	dist_cpu[curr_socket] = cos_cpuid();
	/* printc("multicast, curr socket %d\n",curr_socket); */
	for (i = 0, inter_socket = 0; i < thd_comp->n_cpu; i++) {
		assert (assign[i] >= 0);
		int socket = assign[i] % NUM_CPU_SOCKETS;
		if (socket == curr_socket) {
			local_cores++;
		} else if (dist_cpu[socket] < 0) {
			dist_cpu[socket] = assign[i];
			inter_socket++;
		} 
	}
	/* printc("multicast, local_cores %d, inter socket %d\n",local_cores, inter_socket); */
	assert(local_cores);
	thd_comp->n_acap = inter_socket + local_cores - 1;
	thd_comp->inter_socket = inter_socket;
	thd_comp->dist_thd_idx = 0;

	if (thd_comp->n_acap == 0) return 0;
	assert(thd_comp->n_acap);
	n_acap = thd_comp->n_acap;
	thd_comp->cpus = malloc(sizeof(int) * n_acap);
	if (thd_comp->cpus == NULL) goto err_mem;

	for (i = 0, cpu_idx = 0; i < NUM_CPU_SOCKETS; i++) {
		if (i == curr_socket || dist_cpu[i] < 0) continue;
		thd_comp->cpus[cpu_idx++] = dist_cpu[i];
	}
	assert(cpu_idx == inter_socket);

	for (i = 0, inter_socket = 0; i < thd_comp->n_cpu; i++) {
		int socket = assign[i] % NUM_CPU_SOCKETS;
		if (socket == curr_socket && assign[i] != cos_cpuid()) {
			printc("core %ld thd %d: master got local socket cpu %d\n",
			       cos_cpuid(), cos_get_thd_id(), assign[i]);
			thd_comp->cpus[cpu_idx++] = assign[i];
		}
	}
	assert(cpu_idx == n_acap);

	return 0;
err_mem:
	printc("par_mgr %ld: Cannot allocate memory for thd %d multicast structure.\n", cos_spd_id(), cos_get_thd_id());
	return -1;
}

/* An assumption here, the core ids are assigned in a way that iterates
 * sockets first. */
/* static inline int  */
/* assign_multicast_bestcase(struct intra_comp *thd_comp) */
/* { */
/* 	int i, curr_socket, inter_socket, n_acap, n_cpu = thd_comp->n_cpu, cpu_idx = 0; */
/* 	/\* Multi-cast *\/ */
/* 	inter_socket = n_cpu / NUM_CORE_PER_SOCKET; */
/* 	if (n_cpu % NUM_CORE_PER_SOCKET == 0) inter_socket--; */

/* 	if (inter_socket > 0) { */
/* 		/\* The number of acaps we need for the master: */
/* 		 * inter-socket acaps + intra-socket acaps. *\/ */
/* 		n_acap = inter_socket + NUM_CORE_PER_SOCKET - 1; */
/* 	} else { */
/* 		/\* No inter-socket acaps. Same as unicast. *\/ */
/* 		return assign_unicast_bestcase(thd_comp); */
/* 	} */
/* 	thd_comp->n_acap = n_acap; */

/* 	if (n_acap == 0) return 0; */

/* 	thd_comp->cpus = malloc(sizeof(int) * n_acap); */
/* 	if (unlikely(thd_comp->cpus == NULL)) return -1; */
/* 	thd_comp->inter_socket = inter_socket; */
/* 	thd_comp->dist_thd_idx = 0; */

/* 	curr_socket = cos_cpuid() % NUM_CPU_SOCKETS;  */
/* 	/\* printc("doing multicasting, curr socket %d\n", curr_socket); *\/ */
/* 	for (i = 0; i < NUM_CPU_SOCKETS; i++) { */
/* 		if (i == curr_socket) continue; */

/* 		thd_comp->cpus[cpu_idx++] = i; */
/* 		if (cpu_idx == inter_socket) break; */
/* 	} */
/* 	assert(cpu_idx == inter_socket); */

/* 	//TODO: ask policy! */
/* 	/\* Assign to the current socket. Here we already know we are */
/* 	 * going to use all the cores in the current socket. *\/ */
/* 	for (i = curr_socket; i < NUM_CPU_COS; i += NUM_CPU_SOCKETS) { */
/* 		if (i == cos_cpuid()) continue; */
/* 		printc("core %ld thd %d: master got local socket cpu %d\n", */
/* 		       cos_cpuid(), cos_get_thd_id(), i); */
/* 		thd_comp->cpus[cpu_idx++] = i; */
/* 	} */

/* 	assert(cpu_idx == n_acap); */

/* 	return 0; */
/* } */

/* Uni-cast approach: master sends individual requests to each core. */
/* Return the number of acaps (which is the number of cpus in
 * addition to the current one) that the client can use. n_request
 * and fn are not used for now. */
/* Multi-cast: master sends one request to each socket. A distribution
 * thread (which is high priority) on that socket will forward
 * requests to corresponding cores in the socket. */
/* When multi-cast is enabled, n_acaps is no longer n_cpu - 1. The
 * par_create function returns # of cpu + # of acaps. */
int 
par_create(int spdid, int n_request)
{
#if NUM_CPU_COS == 1
	return 1 << 16; // n_cpu is high 16 bits.
#endif
	int curr = cos_get_thd_id(), n_acap, ret;
	struct thd_intra_comp *curr_thd;
	struct intra_comp *thd_comp;
	
	/* if (unlikely(n_request <= 0)) return 0; */

	if (thd_intra_comp[curr] == NULL) {
		thd_intra_comp[curr] = malloc(sizeof(struct thd_intra_comp));
		if (unlikely(thd_intra_comp[curr] == NULL)) goto err_mem;
	}
	curr_thd = thd_intra_comp[curr];
	assert(curr_thd);

	if (curr_thd->comp[spdid] == NULL) {
		curr_thd->comp[spdid] = malloc(sizeof(struct intra_comp));
		if (unlikely(curr_thd->comp[spdid] == NULL)) goto err_mem;
		curr_thd->comp[spdid]->spdid = spdid;
		curr_thd->comp[spdid]->thdid = curr;
	}
	thd_comp = curr_thd->comp[spdid];

	if (thd_comp->n_cpu == 0) {
		// TODO: policy should make the decision here. Look up a table?
		int ncpu = 0;
		assert(assign[0] == cos_cpuid()); // master thread should be on the first core!
		while (assign[ncpu] >= 0) {
			assert(assign[ncpu] < NUM_CPU_COS);
			ncpu++;
		}
		printc("par_mgr: OMP thread %d getting %d cores\n", curr, ncpu);
		assert(ncpu <= NUM_CPU_COS);
		thd_comp->n_cpu = ncpu;
		assert(ncpu);
#ifdef MULTICAST
		ret = assign_multicast(thd_comp);
#else
		ret = assign_unicast(thd_comp);
#endif
		if (unlikely(ret < 0)) goto err_mem;
	} else {
		/* Created before. We should only call create
		 * once. This only happens when we restarted the
		 * program somehow, e.g. rebooted the component.*/
		assert(thd_comp->n_acap);
	}
	ret = thd_comp->n_cpu << 16 | thd_comp->n_acap;

	return ret;
err_mem:
	printc("par_mgr %ld: Cannot allocate memory for thd %d.\n", cos_spd_id(), curr);
	return -1;
}

void *
par_ring_lookup(int spdid, int n, int nest_level)
{
	int ncpu, curr = cos_get_thd_id();
	struct thd_intra_comp *curr_thd;
	struct intra_comp *thd_comp;
	struct nested_par_info *curr_par;

	/* printc("ring lookup spd %d, n %d, nest %d\n", spdid, n, nest_level); */
	if (unlikely(nest_level >= MAX_OMP_NESTED_PAR_LEVEL || nest_level < 0)) goto err; 

	if (unlikely(thd_intra_comp[curr] == NULL)) goto err;
	curr_thd = thd_intra_comp[curr];

	if (unlikely(curr_thd->comp[spdid] == NULL)) goto err;
	thd_comp = curr_thd->comp[spdid];

	if (unlikely(thd_comp->n_acap <= n)) goto err;
	curr_par = &thd_comp->nested_par[nest_level];

	if (unlikely(curr_par == NULL || curr_par->cap_info == NULL)) goto err;

	return (void *)curr_par->cap_info[n].cap_ring;
err:
	printc("parallel mgr: thread %d lookup ring failed.\n", cos_get_thd_id());
	return NULL;
}

/* Returns wakeup_acap + wait_acap. */
int 
par_acap_get_barrier(int spdid, int nest_level)
{
	int curr = cos_get_thd_id();
	struct thd_intra_comp *curr_thd;
	struct intra_comp *thd_comp;
	int cspd, sspd, ret;
	struct nested_par_info *curr_par;

#ifdef PAR_BARRIER_SPIN
	/* This means we want the master thread to spin on barrier
	 * synchronization. */
	return 0;
#endif
	if (unlikely(thd_intra_comp[curr] == NULL)) goto err;
	curr_thd = thd_intra_comp[curr];

	if (unlikely(curr_thd->comp[spdid] == NULL)) goto err;
	thd_comp = curr_thd->comp[spdid];
	assert(thd_comp && thd_comp->spdid);
	assert(thd_comp->spdid == spdid);

	/* no need to do wait if the current thread is assigned only 1 core. */
	if (unlikely(thd_comp->n_cpu <= 1)) goto err;
	curr_par = &thd_comp->nested_par[nest_level];
	assert(curr_par);

	if (curr_par->wait_acap > 0) {
		assert(curr_par->wakeup_acap > 0);
		ret = curr_par->wakeup_acap << 16 | curr_par->wait_acap;
		goto done;
	}
	cspd = thd_comp->spdid;
	sspd = thd_comp->spdid;
	assert(cspd && sspd);

	/* create acap between cspd and sspd. No owner of this acap
	 * since every child could be invoking it. */
	ret = cos_async_cap_cntl(COS_ACAP_CREATE, cspd, sspd, curr);

	curr_par->wakeup_acap = ret >> 16;
	if (unlikely(curr_par->wakeup_acap <= 0)) { 
		printc("Parallel mgr: client acap creation failed.");
		goto err; 
	}

	curr_par->wait_acap = ret & 0xFFFF;
	if (unlikely(curr_par->wait_acap <= 0)) {
		printc("Parallel mgr: Server acap allocation failed for thread %d.\n", curr);
		goto err;
	}
done:
	return ret;
err:
	return 0;
}

static inline int 
intra_acap_setup(struct intra_comp *comp, int nest_level, int i, const int spin, int thd_init_idx)
{
	int cli_acap, cspd, sspd, thd_id, cpu, ret, j;
	struct srv_thd_info *srv_thd;
	int srv_acap, srv_thd_id;
	struct nested_par_info *par_team;

	/* printc("acap setup: thd %d on core %ld, nest %d, i %d\n",  */
	/*        thd_id, cos_cpuid(), nest_level, i); */
	assert(comp && comp->spdid);
	cspd = comp->spdid;
	sspd = comp->spdid;
	thd_id = comp->thdid;

	assert(comp->cpus);
	if (comp->dist_thd_idx == 0) { assert(i < comp->n_cpu); }// true for non-distribution thread.

	cpu = comp->cpus[i];
	par_team = &comp->nested_par[nest_level];
	assert(par_team->cap_info);

	/* create server thd and wire. */
	/* printc("thd %d going to create thd on cpu %d, spd %d...\n", thd_id, cpu, sspd); */
	srv_thd_id = create_thd_curr_prio(cpu, sspd, thd_init_idx);
	/* printc("thd %d: got new thd id %d\n", thd_id, srv_thd_id); */
	/* printc("Created handling thread %d on cpu %d\n", srv_thd_id, cpu); */
	srv_thd = &srv_thd_info[srv_thd_id];
	srv_thd->cli_spd_id = cspd;
	srv_thd->srv_spd_id = sspd;

	if (!spin) {
		/* create acap between cspd and sspd */
		ret = cos_async_cap_cntl(COS_ACAP_CREATE, cspd, sspd, srv_thd_id);

		cli_acap = ret >> 16;
		if (cli_acap <= 0) { 
			printc("err: async cap creation failed.");
			goto err; 
		}
		par_team->cap_info[i].acap = cli_acap;

		srv_acap = ret & 0xFFFF;
		if (unlikely(srv_acap <= 0)) {
			printc("Parallel mgr: Server acap allocation failed for thread %d.\n", srv_thd_id);
			goto err;
		}
		srv_thd->srv_acap = srv_acap;
	} else {
		/* This means the worker thread spins to wait for job
		 * creation. Thus no need to create acaps. */
		par_team->cap_info[i].acap = -1;
		srv_thd->srv_acap = -1;
	}

	for (j = 0; assign[j] != cpu; j++) assert(j <= NUM_CPU_COS);

	/* 0 is the main thread. When we use multicast, the first n
	 * (n=inter-socket) acaps are not for real worker
	 * threads. Thus thd_num is not simply the index i. */
	if (comp->dist_thd_idx > 0) {
		/* Means this is the distribution thread. */
		struct srv_thd_info *dist_thd = &srv_thd_info[thd_id];

		assert(comp->inter_socket == -1 && comp->n_cpu == -1);
		assert(nest_level == 0); //dist thread itself has no nest level
		srv_thd->parent = dist_thd->parent;
		srv_thd->nesting_level = dist_thd->nesting_level;
		srv_thd->thd_num = j; // i + comp->dist_thd_idx * NUM_CORE_PER_SOCKET;
		printc("dist thd %d got srv thd %d on cpu %d, thd num %d - parent %d, nest %d\n",
		       thd_id, srv_thd_id, cpu, srv_thd->thd_num, srv_thd->parent, srv_thd->nesting_level);
	} else {
		srv_thd->parent = thd_id;
		srv_thd->nesting_level = nest_level;
		assert(i >= comp->inter_socket);
		srv_thd->thd_num = j; // i - comp->inter_socket + 1;
	}

	par_team->cap_info[i].cap_srv_thd = srv_thd_id;

	ret = intra_shared_page_setup(srv_thd_id, &par_team->cap_info[i]);
	if (ret < 0) {
		printc("Parallel mgr: ring buffer allocation error!\n");
		goto err;
	}

	/* Upcall thread will be blocking on initialization. */
	if (sched_wakeup(cos_spd_id(), srv_thd_id)) BUG(); // wakeup and block should be within the same comp!!!
	
	return 0;
err:
	return -1;
}

static inline int dist_thread_create(int dist_thd_id, int spdid, int n) 
{
	int i, n_acap, curr_socket, local_cores, cpu_idx;
	struct thd_intra_comp *dist_thd_comps;
	struct intra_comp *dist_thd, *parent;
	
	thd_intra_comp[dist_thd_id] = malloc(sizeof(struct thd_intra_comp));
	if (unlikely(thd_intra_comp[dist_thd_id] == NULL)) goto err_mem;
	dist_thd_comps = thd_intra_comp[dist_thd_id];

	dist_thd_comps->comp[spdid] = malloc(sizeof(struct intra_comp));
	if (unlikely(dist_thd_comps->comp[spdid] == NULL)) goto err_mem;
	dist_thd = dist_thd_comps->comp[spdid];
	dist_thd->spdid = spdid;
	dist_thd->thdid = dist_thd_id;

	dist_thd->dist_thd_idx = n + 1;   /* means it's the (n+1)th dist thread */
	dist_thd->inter_socket = -1;      /* meaningless for dist thd. Sanity check only */
	dist_thd->n_cpu = -1;       

	parent = thd_intra_comp[cos_get_thd_id()]->comp[spdid];
	assert(n < parent->inter_socket);

	curr_socket = parent->cpus[n] % NUM_CPU_SOCKETS; 

	for (i = 0, local_cores = 0; i < parent->n_cpu; i++) {
		int socket = assign[i] % NUM_CPU_SOCKETS;
		if (socket == curr_socket) local_cores++;
	}

	dist_thd->n_acap = local_cores;
	n_acap = dist_thd->n_acap;
	assert(n_acap);

	dist_thd->cpus = malloc(sizeof(int) * n_acap);
	if (unlikely(dist_thd->cpus == NULL)) goto err_mem;

	for (i = 0, cpu_idx = 0; i < parent->n_cpu; i++) {
		int socket = assign[i] % NUM_CPU_SOCKETS;
		if (socket == curr_socket) dist_thd->cpus[cpu_idx++] = assign[i];
	}

	return 0;
err_mem:
	printc("par_mgr %ld: Cannot allocate memory for thd %d.\n", cos_spd_id(), cos_get_thd_id());
	return -1;
}

/* static inline int dist_thread_create_bestcase(int dist_thd_id, int spdid, int n)  */
/* { */
/* 	int i, n_acap, socket, cpu_idx = 0; */
/* 	struct thd_intra_comp *dist_thd_comps; */
/* 	struct intra_comp *dist_thd, *parent; */
	
/* 	thd_intra_comp[dist_thd_id] = malloc(sizeof(struct thd_intra_comp)); */
/* 	if (unlikely(thd_intra_comp[dist_thd_id] == NULL)) goto err_mem; */
/* 	dist_thd_comps = thd_intra_comp[dist_thd_id]; */

/* 	dist_thd_comps->comp[spdid] = malloc(sizeof(struct intra_comp)); */
/* 	if (unlikely(dist_thd_comps->comp[spdid] == NULL)) goto err_mem; */
/* 	dist_thd = dist_thd_comps->comp[spdid]; */
/* 	dist_thd->spdid = spdid; */
/* 	dist_thd->thdid = dist_thd_id; */

/* 	dist_thd->dist_thd_idx = n + 1;   /\* means it's the (n+1)th dist thread *\/ */
/* 	dist_thd->inter_socket = -1;     /\* meaningless for dist thd. Sanity check only *\/ */
/* 	dist_thd->n_cpu = -1;        */

/* 	parent = thd_intra_comp[cos_get_thd_id()]->comp[spdid]; */
/* 	assert(n < parent->inter_socket); */

/* 	if (n == parent->inter_socket - 1) { */
/* 		/\* means the last socket the parent has. *\/ */
/* 		if (parent->n_cpu % NUM_CORE_PER_SOCKET == 0) */
/* 			dist_thd->n_acap = NUM_CORE_PER_SOCKET; */
/* 		else  */
/* 			dist_thd->n_acap = parent->n_cpu % NUM_CORE_PER_SOCKET; */
/* 	} else { */
/* 		/\* we have all cores in this socket. *\/ */
/* 		dist_thd->n_acap = NUM_CORE_PER_SOCKET; */
/* 	} */
/* 	n_acap = dist_thd->n_acap; */
/* 	assert(n_acap); */

/* 	dist_thd->cpus = malloc(sizeof(int) * n_acap); */
/* 	if (unlikely(dist_thd->cpus == NULL)) return -1; */

/* 	socket = parent->cpus[n];  */
/* 	for (i = socket; i < socket + n_acap * NUM_CPU_SOCKETS; i += NUM_CPU_SOCKETS) { */
/* 		assert(i <= NUM_CPU_COS); */
/* 		dist_thd->cpus[cpu_idx++] = i; */
/* 		assert(cpu_idx <= n_acap); */
/* 	} */

/* 	return 0; */
/* err_mem: */
/* 	printc("par_mgr %ld: Cannot allocate memory for thd %d.\n", cos_spd_id(), cos_get_thd_id()); */
/* 	return -1; */
/* } */

#define IPI_DIST_PRIO 2

static inline int 
distribution_acap_setup(struct intra_comp *comp, int nest_level, int i, int thd_init_idx)
{
	int cli_acap, cspd, sspd, thd_id, cpu, ret, j;
	struct srv_thd_info *srv_thd;
	int srv_acap, srv_thd_id;
	struct nested_par_info *par_team;
	struct intra_comp *dist_thd;

	/* printc("acap setup: thd %d on core %ld, nest %d, i %d\n",  */
	/*        cos_get_thd_id(), cos_cpuid(), nest_level, i); */
	assert(comp && comp->spdid);
	cspd = comp->spdid;
	sspd = comp->spdid;
	thd_id = cos_get_thd_id();

	assert(comp->cpus && i < comp->n_cpu);
	cpu = comp->cpus[i];

	par_team = &comp->nested_par[nest_level];
	assert(par_team->cap_info);

	/* create server thd */
	/* printc("thd %d going to create thd on cpu %d, spd %d...\n", cos_get_thd_id(), cpu, sspd); */
	srv_thd_id = create_thd_prio(cpu, IPI_DIST_PRIO, sspd, thd_init_idx);
	/* printc("thd %d: got new thd id %d\n", cos_get_thd_id(), srv_thd_id); */
	printc("Created distribution thread %d on cpu %d @ prio %d\n", srv_thd_id, cpu, IPI_DIST_PRIO);
	srv_thd = &srv_thd_info[srv_thd_id];
	srv_thd->cli_spd_id = cspd;
	srv_thd->srv_spd_id = sspd;

	srv_thd->parent = thd_id;
	srv_thd->thd_num = 0; /* Distribution thread has no thd num. */
	srv_thd->nesting_level = nest_level;
	par_team->cap_info[i].cap_srv_thd = srv_thd_id;

	ret = dist_thread_create(srv_thd_id, cspd, i);
	if (ret < 0) goto err;

	ret = intra_shared_page_setup(srv_thd_id, &par_team->cap_info[i]);
	if (ret < 0) {
		printc("Parallel mgr: ring buffer allocation error!\n");
		goto err;
	}

	/* create acap between cspd and sspd */
	ret = cos_async_cap_cntl(COS_ACAP_CREATE, cspd, sspd, srv_thd_id); // setup should be one call
	cli_acap = ret >> 16;
	if (cli_acap <= 0) { 
		printc("err: async cap creation failed.");
		goto err; 
	}
	par_team->cap_info[i].acap = cli_acap;

	srv_acap = ret & 0xFFFF;
	if (unlikely(srv_acap <= 0)) {
		printc("Parallel mgr: Server acap allocation failed for thread %d.\n", srv_thd_id);
		goto err;
	}
	srv_thd->srv_acap = srv_acap;

	/* This should be created lazily by the dist thread
	 * itself. However I need the priority information so that we
	 * can create worker threads at the same prio as the
	 * master. Dist thread has high priority. So we let the master
	 * create those worker threads for the dist thread. */
	dist_thd = thd_intra_comp[srv_thd_id]->comp[cspd];
	assert(dist_thd->n_acap);
	dist_thd->nested_par[0].cap_info = malloc(sizeof(struct per_cap_thd_info) * (dist_thd->n_acap));
	if (dist_thd->nested_par[0].cap_info == NULL) {
		printc("cos parallel mgr: could not allocate memory for cap struct.\n");
		goto err;
	}
	for (j = 0; j < dist_thd->n_acap; j++) {
#ifdef PAR_CREATION_SPIN
		ret = intra_acap_setup(dist_thd, 0, j, 1, thd_init_idx);
#else
		ret = intra_acap_setup(dist_thd, 0, j, 0, thd_init_idx);
#endif
		assert(ret == 0);
	}

	/* Upcall thread will be blocking on initialization. */
	if (sched_wakeup(cos_spd_id(), srv_thd_id)) BUG(); // wakeup and block should be within the same comp!!!
	
	return 0;
err:
	return -1;
}

/* Now support nested parallel. */
int 
par_acap_lookup(int spdid, int n, int nest_level, int thd_init_idx)
{
	int ncpu, curr = cos_get_thd_id();
	struct thd_intra_comp *curr_thd;
	struct intra_comp *thd_comp;
	struct nested_par_info *curr_par;
	struct per_cap_thd_info *curr_cap;

	if (unlikely(thd_intra_comp[curr] == NULL)) goto err;
	curr_thd = thd_intra_comp[curr];

	if (unlikely(curr_thd->comp[spdid] == NULL)) goto err;
	if (unlikely(nest_level > MAX_OMP_NESTED_PAR_LEVEL || nest_level < 0)) goto err; 

	thd_comp = curr_thd->comp[spdid];
	if (unlikely(n > thd_comp->n_acap)) goto err;
	if (n == thd_comp->n_acap) {
		/* The IPI distribution thread see this as a lookup
		 * terminator. */
		return 0;
	}

	curr_par = &thd_comp->nested_par[nest_level];
	if (curr_par->cap_info == NULL) {
		curr_par->cap_info = malloc(sizeof(struct per_cap_thd_info) * (thd_comp->n_acap));
		if (unlikely(curr_par->cap_info == NULL)) {
			printc("cos parallel mgr: could not allocate memory for cap struct.\n");
			goto err;
		}
	}
	    
	if (curr_par->cap_info[n].acap == 0) {
		int ret = 0;
		if (n < thd_comp->inter_socket) {
			/* This means the master is looking for the
			 * IPI distribution acap. */
			assert(thd_comp->inter_socket > 0);
			ret = distribution_acap_setup(thd_comp, nest_level, n, thd_init_idx);
		} else {
#ifdef PAR_CREATION_SPIN
			ret = intra_acap_setup(thd_comp, nest_level, n, 1, thd_init_idx);
#else
			ret = intra_acap_setup(thd_comp, nest_level, n, 0, thd_init_idx);
#endif
		}
		assert(ret == 0);
	}

	assert(curr_par->cap_info[n].acap);
	return curr_par->cap_info[n].acap;
err:
	printc("parallel manager: thd %d client acap lookup failed.\n", cos_get_thd_id());
	return -1;
}

/* Returns thd_num for the current server thread. */
int par_srv_thd_num_lookup(int spdid) {
	int acap, thd_id = cos_get_thd_id();
	struct srv_thd_info *thd;
	thd = &srv_thd_info[thd_id];
	assert(thd);
 	if (unlikely(thd->srv_spd_id != spdid)) goto err_spd;
	
	return thd->thd_num;
err_spd:
	printc("par_mgr: upcall thread calling lookup from wrong component %d.\n", spdid);
	return -1;
}

/* Returns the parent thread id + nesting level. */
int par_parent_lookup(int spdid) {
	int acap, thd_id = cos_get_thd_id();
	struct srv_thd_info *thd;

	thd = &srv_thd_info[thd_id];
	assert(thd);
 	if (unlikely(thd->srv_spd_id != spdid)) goto err_spd;

	if (thd_intra_comp[thd_id] && thd_intra_comp[thd_id]->comp[spdid]
	    && thd_intra_comp[thd_id]->comp[spdid]->dist_thd_idx > 0) {
		/* This means the current thread is a IPI distribution
		 * thread. Return 0 to notify it so that it calls the
		 * distribution function in the client lib. */
		return 0;
	}
	
	return thd->parent << 16 | thd->nesting_level;
err_spd:
	printc("par_mgr: upcall thread calling lookup from wrong component %d.\n", spdid);
	return -1;
}

/* External functions done. */

int redirect_cap_async(int cspd, int sspd) 
{
	int i, ret = 0, acap, acap_id, thd_id;
	struct static_cap *cap;

	if (unlikely(cspd > MAX_NUM_SPDS || cspd < 0 ||
		     sspd > MAX_NUM_SPDS || sspd < 0)) {
		printc("err: spd %d or %d doesn't exist.\n", cspd, sspd);
		goto err;
	}

	if (unlikely(comp_info[cspd].cap == NULL)) {
		assert(comp_info[cspd].ncaps == 0);
		printc("err: spd %d doesn't exist or has no static caps.\n", cspd);
		goto err;
	}
	
	for (i = 0; i < comp_info[cspd].ncaps; i++) {
		cap = &(comp_info[cspd].cap[i]);
		if (cap->srv_comp != sspd) continue;
		printc("linking cap %d of cspd %d\n", i, cspd);
		ret = cos_async_cap_cntl(COS_ACAP_LINK_STATIC_CAP, cspd, i, 0);

		if (ret < 0) { 
			printc("err: linking cap %d to acap stub failed (comp %d).", i, cspd);
			goto err; 
		}
	}

	printc("ret from acap creation %d.\n", ret);

	return 0;
err:
	return -1;
}

static inline int init_comp(int cspd) 
{
	int i, ncaps;
	struct static_cap *cap;

	assert(comp_info[cspd].cap == NULL);

	comp_info[cspd].comp_id = cspd;
	ncaps = cos_cap_cntl(COS_CAP_GET_SPD_NCAPS, cspd, 0, 0);

	if (ncaps <= 0) {
		comp_info[cspd].ncaps = 0;
		goto done;
	}

	comp_info[cspd].ncaps = ncaps;

	comp_info[cspd].cap = malloc(sizeof(struct static_cap) * (ncaps + 1));
	if (unlikely(comp_info[cspd].cap == NULL)) goto err;

	for (i = 1; i < ncaps; i++) { // cap 0 is for return. not valid here.
		cap = &(comp_info[cspd].cap[i]);
		cap->srv_comp = cos_cap_cntl(COS_CAP_GET_DEST_SPD, cspd, i, 0);
		assert(cap->srv_comp > 0);
		cap->srv_fn = (void *)cos_cap_cntl(COS_CAP_GET_DEST_FN, cspd, i, 0);
		assert(cap->srv_fn);
	}
done:
	/* printc("init comp %d (ncaps %d)done\n", cspd, ncaps); */
	return 0;
err:
	printc("par_mgr: cannot allocate memory for acap mapping structure.");
	return -1;
}

static inline int init_par_mgr (void) 
{
	int i;
	
	memset(comp_info, 0, sizeof(struct comp_info) * MAX_NUM_SPDS);
	memset(srv_thd_info, 0, sizeof(struct srv_thd_info) * MAX_NUM_THREADS);
	memset(thd_intra_comp, 0, sizeof(struct thd_intra_comp *) * MAX_NUM_THREADS);

	for (i = 1; i < MAX_NUM_SPDS; i++) {
		init_comp(i);
	}
	
	return 0;
}

void cos_init(void) 
{
	struct srv_thd_info *srv_thd;
	int srv_spd;

	/* printc("PAR_mgr init: thd %d, core %ld\n", cos_get_thd_id(), cos_cpuid()); */
	init_par_mgr();
/* #define PING_SPDID 9 */
/* #define PONG_SPDID 8 */
/* 	int cspd = PING_SPDID, sspd = PONG_SPDID; */
	/* redirect_cap_async(cspd, sspd); */

	printc("PAR_mgr init done. thd %d, core %ld\n", cos_get_thd_id(), cos_cpuid());

	return;
}
