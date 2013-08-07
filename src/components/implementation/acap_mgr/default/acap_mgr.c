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

#include <acap_mgr.h>
#include <acap_mgr_intra.h>
#include <bitmap.h>

struct srv_thd_info { 
	/* information of handling thread */
	int cli_spd_id;
	int srv_spd_id;
	int srv_acap; /* the server side acap that we do ainv_wait on. */
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


static int create_thd_curr_prio(int core, int spdid) 
{
	union sched_param sp, sp1;
	int ret;
	sp.c.type = SCHEDP_RPRIO;
	sp.c.value = 0;

	sp1.c.type = SCHEDP_CORE_ID;
	sp1.c.value = core;
	ret = sched_create_thd(spdid, sp.v, sp1.v, 0);

	if (!ret) BUG();

	return ret;
}

/* static int create_thd(int core, int prio, int spdid) { */
/* 	union sched_param sp, sp1; */
/* 	int ret; */

/* 	sp.c.type = SCHEDP_PRIO; */
/* 	sp.c.value = prio; */

/* 	sp1.c.type = SCHEDP_CORE_ID; */
/* 	sp1.c.value = core; */
/* 	ret = sched_create_thd(spdid, sp.v, sp1.v, 0); */

/* 	if (!ret) BUG(); */

/* 	return ret; */
/* } */

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
		printc("acap_mgr: alloc ring buffer failed in mgr %ld.\n", cos_spd_id());
		goto err;
	}

	srv_thd_info[thd_id].mgr_ring = ring_mgr;

	ring_cli = (vaddr_t)valloc_alloc(cos_spd_id(), cspd, 1);
	if (unlikely(!ring_cli)) {
		printc("acap_mgr: vaddr alloc failed in client comp %d.\n", cspd);
		goto err_cli;
	}
	if (unlikely(ring_cli != mman_alias_page(cos_spd_id(), ring_mgr, cspd, ring_cli))) {
		printc("acap_mgr: alias to client %d failed.\n", cspd);
		goto err_cli_alias;
	}
	comp_info[cspd].cli_thd_info[cos_get_thd_id()]->cap_info[thd->cli_cap_id].cap_ring = ring_cli;
	
	ring_srv = (vaddr_t)valloc_alloc(cos_spd_id(), sspd, 1);
	if (unlikely(!ring_srv)) {
		goto err_srv;
		printc("acap_mgr: vaddr alloc failed in server comp  %d.\n", sspd);
	}
	if (unlikely(ring_srv != mman_alias_page(cos_spd_id(), ring_mgr, sspd, ring_srv))) {
		printc("acap_mgr: alias to server %d failed.\n", sspd);
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
	printc("acap_mgr: upcall thread calling lookup from wrong component %d.\n", spdid);
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
	printc("acap_mgr: upcall thread calling lookup from wrong component %d.\n", spdid);
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
int acap_cli_lookup(int spdid, int cap_id) 
{
	int srv_thd_id, acap_id, acap_v, srv_acap, cspd, sspd;
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

	/* create acap between cspd and sspd */
	acap_id = cos_async_cap_cntl(COS_ACAP_CLI_CREATE, cspd, sspd, thd_id);
	if (acap_id <= 0) { 
		printc("err: async cap creation failed.");
		goto err; 
	}
	acap_v = acap_id | COS_ASYNC_CAP_FLAG;

	/* create server thd and wire. */
	srv_thd_id = create_thd_curr_prio(cpu, sspd);
	/* printc("Created handling thread %d on cpu %d\n", srv_thd_id, cpu); */
	srv_thd = &srv_thd_info[srv_thd_id];
	srv_thd->cli_spd_id = cspd;
	srv_thd->srv_spd_id = sspd;

	srv_thd->cli_thd = thd_id;
	srv_thd->cli_cap_id = cap_id;
	cli_thd_info->cap_info[cap_id].cap_srv_thd = srv_thd_id;

	int ret = 0;
	ret = shared_page_setup(srv_thd_id);
	if (ret < 0) {
		printc("acap_mgr: ring buffer allocation error!\n");
		goto err;
	}

	cli_thd_info->cap_info[cap_id].acap = acap_v; /* client side acap */

	srv_acap = cos_async_cap_cntl(COS_ACAP_SRV_CREATE, sspd, srv_thd_id, 0);
	if (unlikely(srv_acap <= 0)) {
		printc("Server acap allocation failed for thread %d.\n", srv_thd_id);
		goto err;
	}
	srv_thd->srv_acap = srv_acap;
	ret = cos_async_cap_cntl(COS_ACAP_WIRE, cspd, acap_id, sspd << 16 | srv_acap);
	if (unlikely(ret)) goto err;
	
	if (sched_wakeup(cos_spd_id(), srv_thd_id)) BUG();

	/* printc("acap_mgr returning acap %d (%d), thd %d\n",  */
	/*        acap_v, acap_v & ~COS_ASYNC_CAP_FLAG, cos_get_thd_id()); */

	return acap_v;
err_cap:
	printc("acap_mgr: thread %d calling lookup for non-existing cap %d in component %d.\n",
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
	int spdid;
	int n_cpu;
	int *cpus;
	struct nested_par_info nested_par[MAX_NESTED_PAR_LEVEL]; 
};

struct thd_intra_comp {
	struct intra_comp *comp[MAX_NUM_SPDS];
};

struct thd_intra_comp *thd_intra_comp[MAX_NUM_THREADS];


static int intra_shared_page_setup(int thd_id, struct per_cap_thd_info *cap_info) 
{
	// thd_id is the server upcall thread
	struct srv_thd_info *thd;
	int cspd, sspd;
	vaddr_t ring_mgr, ring_cli, ring_srv;

	assert(thd_id);

	thd = &srv_thd_info[thd_id];
	cspd = thd->cli_spd_id;
	sspd = thd->srv_spd_id;
	assert(cspd == sspd);
	if (!cspd || !sspd) goto err;

	ring_mgr = (vaddr_t)alloc_page();
	if (!ring_mgr) {
		printc("acap_mgr: alloc ring buffer failed in mgr %ld.\n", cos_spd_id());
		goto err;
	}

	srv_thd_info[thd_id].mgr_ring = ring_mgr;

	ring_cli = (vaddr_t)valloc_alloc(cos_spd_id(), cspd, 1);
	if (unlikely(!ring_cli)) {
		printc("acap_mgr: vaddr alloc failed in client comp %d.\n", cspd);
		goto err_cli;
	}
	if (unlikely(ring_cli != mman_alias_page(cos_spd_id(), ring_mgr, cspd, ring_cli))) {
		printc("acap_mgr: alias to client %d failed.\n", cspd);
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
		     leqpow2((PAGE_SIZE - CACHE_LINE - sizeof(CK_RING_INSTANCE(intra_inv_ring))) / sizeof(struct intra_inv_data))); 
	
	return 0;

err_cli_alias:
	valloc_free(cos_spd_id(), cspd, (void *)ring_cli, 1);
err_cli:
	free_page((void *)ring_mgr);
err:	
	return -1;
}

/* Return the number of acaps (which is the number of cpus in
 * addition to the current one) that the client can use. n_request
 * and fn are not used for now. */
int 
par_create(int spdid, int n_request)
{
	int ncpu, curr = cos_get_thd_id(), n_acap;
	struct thd_intra_comp *curr_thd;
	struct intra_comp *thd_comp;
	
	if (unlikely(n_request <= 0)) return 0;

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
	}
	thd_comp = curr_thd->comp[spdid];

	if (thd_comp->n_cpu == 0) {
		// TODO: policy should make the decision here. Look up a table?
		/* if (n_request > NUM_CPU_COS)  */
		/* 	thd_comp->n_cpu = NUM_CPU_COS; */
		/* else */
		/* 	thd_comp->n_cpu = n_request; */
		thd_comp->n_cpu = NUM_CPU_COS;

		/* printc("request %d, n_cpu %d\n", n_request, thd_comp->n_cpu); */
		n_acap = thd_comp->n_cpu - 1;
		if (n_acap > 0) {
			thd_comp->cpus = malloc(sizeof(int) * n_acap);
			if (unlikely(thd_comp->cpus == NULL)) goto err_mem;

			int i, j = 0; //TODO: ask policy!
			for (i = 0; i < NUM_CPU_COS; i++) {
				if (i == cos_cpuid()) continue;
				/* printc("core %ld thd %d: got extra cpu %d\n",  */
				/*        cos_cpuid(), cos_get_thd_id(), i); */
				thd_comp->cpus[j++] = i;
				if (j == n_acap) break;
			}
			assert(j == n_acap);
		}
	}

	return n_acap;
err_mem:
	printc("acap_mgr %ld: Cannot allocate memory for thd %d.\n", cos_spd_id(), curr);
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
	if (unlikely(nest_level > MAX_NESTED_PAR_LEVEL || nest_level < 0)) goto err; 

	if (unlikely(thd_intra_comp[curr] == NULL)) goto err;
	curr_thd = thd_intra_comp[curr];

	if (unlikely(curr_thd->comp[spdid] == NULL)) goto err;
	thd_comp = curr_thd->comp[spdid];

	if (unlikely(thd_comp->n_cpu <= n)) goto err;
	curr_par = &thd_comp->nested_par[nest_level];
	if (unlikely(curr_par->cap_info == NULL)) goto err;

	return (void *)curr_par->cap_info[n].cap_ring;
err:
	printc("parallel mgr: thread %d lookup ring failed.\n", cos_get_thd_id());
	return NULL;
}

/* Returns wait_acap + wakeup_acap. */
int 
par_get_wait_wakeup_acap(int spdid, int nest_level)
{
	int curr = cos_get_thd_id();
	struct thd_intra_comp *curr_thd;
	struct intra_comp *thd_comp;
	int cspd, sspd, cpu, ret;
	struct nested_par_info *curr_par;

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

	if (curr_par->wait_acap > 0) goto done;
	cspd = thd_comp->spdid;
	sspd = thd_comp->spdid;
	assert(cspd && sspd);

	cpu = cos_cpuid();
	/* create acap between cspd and sspd. No owner of this acap
	 * since every child could be invoking it. */
	curr_par->wakeup_acap = cos_async_cap_cntl(COS_ACAP_CLI_CREATE, cspd, sspd, 0);
	if (unlikely(curr_par->wakeup_acap <= 0)) { 
		printc("err: async cap creation failed.");
		goto err; 
	}

	curr_par->wait_acap = cos_async_cap_cntl(COS_ACAP_SRV_CREATE, sspd, curr, 0);
	if (unlikely(curr_par->wait_acap <= 0)) {
		printc("Server acap allocation failed for thread %d.\n", curr);
		goto err;
	}
	ret = cos_async_cap_cntl(COS_ACAP_WIRE, cspd, curr_par->wakeup_acap, 
				 sspd << 16 | curr_par->wait_acap);
	if (unlikely(ret)) goto err;
done:
	return curr_par->wait_acap << 16 | curr_par->wakeup_acap;
err:
	return 0;
}

static inline int 
intra_acap_setup(struct intra_comp *comp, int nest_level, int i)
{
	int acap_id, cspd, sspd, thd_id, cpu;
	struct srv_thd_info *srv_thd;
	int srv_acap, srv_thd_id, acap_v;
	struct nested_par_info *par_team;

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

	/* create acap between cspd and sspd */
	acap_id = cos_async_cap_cntl(COS_ACAP_CLI_CREATE, cspd, sspd, thd_id);
	if (acap_id <= 0) { 
		printc("err: async cap creation failed.");
		goto err; 
	}
	acap_v = acap_id | COS_ASYNC_CAP_FLAG;
	/* client side acap. We need the acap value here instead of
	 * the id, because client makes async inv on this acap. */
	par_team->cap_info[i].acap = acap_v;

	/* create server thd and wire. */
	/* printc("thd %d going to create thd on cpu %d, spd %d...\n", cos_get_thd_id(), cpu, sspd); */
	srv_thd_id = create_thd_curr_prio(cpu, sspd);
	/* printc("thd %d: got new thd id %d\n", cos_get_thd_id(), srv_thd_id); */
	/* printc("Created handling thread %d on cpu %d\n", srv_thd_id, cpu); */
	srv_thd = &srv_thd_info[srv_thd_id];
	srv_thd->cli_spd_id = cspd;
	srv_thd->srv_spd_id = sspd;

	srv_thd->cli_thd = thd_id;
	srv_thd->cli_cap_id = 0;
	srv_thd->thd_num = i + 1; /* 0 is the main thread. */
	srv_thd->parent = thd_id;
	srv_thd->nesting_level = nest_level;
	par_team->cap_info[i].cap_srv_thd = srv_thd_id;

	int ret = 0;
	ret = intra_shared_page_setup(srv_thd_id, &par_team->cap_info[i]);
	if (ret < 0) {
		printc("acap_mgr: ring buffer allocation error!\n");
		goto err;
	}

	srv_acap = cos_async_cap_cntl(COS_ACAP_SRV_CREATE, sspd, srv_thd_id, 0);
	if (unlikely(srv_acap <= 0)) {
		printc("Server acap allocation failed for thread %d.\n", srv_thd_id);
		goto err;
	}
	srv_thd->srv_acap = srv_acap;
	ret = cos_async_cap_cntl(COS_ACAP_WIRE, cspd, acap_id, sspd << 16 | srv_acap);
	if (unlikely(ret)) goto err;
	
	if (sched_wakeup(cos_spd_id(), srv_thd_id)) BUG();
	
	return 0;
err:
	return -1;
}

/* Now support nested parallel. */
int 
par_acap_lookup(int spdid, int n, int nest_level)
{
	int ncpu, curr = cos_get_thd_id();
	struct thd_intra_comp *curr_thd;
	struct intra_comp *thd_comp;
	struct nested_par_info *curr_par;
	struct per_cap_thd_info *curr_cap;

	if (unlikely(thd_intra_comp[curr] == NULL)) goto err;

	curr_thd = thd_intra_comp[curr];
	if (unlikely(curr_thd->comp[spdid] == NULL)) goto err;

	thd_comp = curr_thd->comp[spdid];
	if (unlikely(n >= (thd_comp->n_cpu - 1))) goto err;
	if (unlikely(nest_level > MAX_NESTED_PAR_LEVEL || nest_level < 0)) goto err; 
	curr_par = &thd_comp->nested_par[nest_level];

	if (curr_par->cap_info == NULL) {
		curr_par->cap_info = malloc(sizeof(struct per_cap_thd_info) * (thd_comp->n_cpu - 1));
		if (unlikely(curr_par->cap_info == NULL)) {
			printc("cos parallel mgr: could not allocate memory for cap struct.\n");
			goto err;
		}
	}
	    
	if (curr_par->cap_info[n].acap == 0) {
		int ret = intra_acap_setup(thd_comp, nest_level, n);
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
 	if (unlikely(thd->srv_spd_id != spdid)) goto err_spd;
	
	return thd->thd_num;
err_spd:
	printc("acap_mgr: upcall thread calling lookup from wrong component %d.\n", spdid);
	return -1;
}

/* Returns the parent thread id + nesting level. */
int par_parent_lookup(int spdid) {
	int acap, thd_id = cos_get_thd_id();
	struct srv_thd_info *thd;
	thd = &srv_thd_info[thd_id];
 	if (unlikely(thd->srv_spd_id != spdid)) goto err_spd;
	
	return thd->parent << 16 | thd->nesting_level;
err_spd:
	printc("acap_mgr: upcall thread calling lookup from wrong component %d.\n", spdid);
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
		ret = cos_async_cap_cntl(COS_ACAP_LINK, cspd, i, 0);

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
	printc("acap_mgr: cannot allocate memory for acap mapping structure.");
	return -1;
}

static inline int init_acap_mgr (void) 
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

	printc("ACAP mgr init: thd %d, core %ld\n", cos_get_thd_id(), cos_cpuid());
	init_acap_mgr();
#define PING_SPDID 9
#define PONG_SPDID 8
	int cspd = PING_SPDID, sspd = PONG_SPDID;

	redirect_cap_async(cspd, sspd);

	printc("ACAP mgr init done. thd %d, core %ld\n", cos_get_thd_id(), cos_cpuid());

	return;
}
