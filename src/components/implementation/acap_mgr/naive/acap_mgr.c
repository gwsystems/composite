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

#include <acap_mgr.h>
#include <valloc.h>
#include <mem_mgr_large.h>


#define PING_SPDID 9
#define PONG_SPDID 8

struct thd_info { 
	/* information of handling thread */
	int cli_spd_id;
	int srv_spd_id;
	volatile int srv_acap; /* the server side acap that we do ainv_wait on. */
	vaddr_t cli_ring;
	vaddr_t srv_ring;
	vaddr_t mgr_ring;
} thd_info[MAX_NUM_THREADS];

struct static_cap {
	int srv_comp;
	int shared_acap;
};

struct thd_acap {
	/* mapping between s_cap and a_cap for a thread within a
	 * specific component. */
	int acap[MAX_STATIC_CAP];
};

struct comp_info {
	int comp_id;
	int ncaps; //number of static caps
	struct static_cap *cap;
	/* acap for each thread in this component. */
	struct thd_acap *thd_acap[MAX_NUM_THREADS];
} comp_info[MAX_NUM_SPDS];


static int create_thd_curr_prio(int core) {
	union sched_param sp, sp1;
	int ret;
	sp.c.type = SCHEDP_RPRIO;
	sp.c.value = 0;

	sp1.c.type = SCHEDP_CORE_ID;
	sp1.c.value = core;
	ret = sched_create_thd(cos_spd_id(), sp.v, sp1.v, 0);

	if (!ret) BUG();

	return ret;
}

/* static int create_thd(int core, int prio) { */
/* 	union sched_param sp, sp1; */
/* 	int ret; */

/* 	sp.c.type = SCHEDP_PRIO; */
/* 	sp.c.value = prio; */

/* 	sp1.c.type = SCHEDP_CORE_ID; */
/* 	sp1.c.value = core; */
/* 	ret = sched_create_thd(cos_spd_id(), sp.v, sp1.v, 0); */

/* 	if (!ret) BUG(); */

/* 	return ret; */
/* } */

static int alloc_ring(int thd_id) {
	// thd_id is the upcall thread on the server side.

	struct thd_info *thd;
	int cspd, sspd;
	vaddr_t ring_mgr, ring_cli, ring_srv;

	assert(thd_id);

	thd = &thd_info[thd_id];
	cspd = thd->cli_spd_id;
	sspd = thd->srv_spd_id;
	
	if (!cspd || !sspd) goto err;

	ring_mgr = (vaddr_t)alloc_page();
	if (!ring_mgr) {
		printc("acap_mgr: alloc ring buffer failed in mgr %ld.\n", cos_spd_id());
		goto err;
	}

	thd_info[thd_id].mgr_ring = ring_mgr;

	ring_cli = (vaddr_t)valloc_alloc(cos_spd_id(), cspd, 1);
	if (unlikely(!ring_cli)) {
		printc("acap_mgr: vaddr alloc failed in client comp %d.\n", cspd);
		goto err_cli;
	}
	if (unlikely(ring_cli != mman_alias_page(cos_spd_id(), ring_mgr, cspd, ring_cli))) {
		printc("acap_mgr: alias to client %d failed.\n", cspd);
		goto err_cli_alias;
	}
	thd_info[thd_id].cli_ring = ring_cli;
	
	ring_srv = (vaddr_t)valloc_alloc(cos_spd_id(), sspd, 1);
	if (unlikely(!ring_srv)) {
		goto err_srv;
		printc("acap_mgr: vaddr alloc failed in server comp  %d.\n", sspd);
	}
	if (unlikely(ring_srv != mman_alias_page(cos_spd_id(), ring_mgr, sspd, ring_srv))) {
		printc("acap_mgr: alias to server %d failed.\n", sspd);
		goto err_srv_alias;
	}
	thd_info[thd_id].srv_ring = ring_srv;
	
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

/* Called by the upcall thread in the server component. */
int acap_srv_mapping_lookup(int spdid, int s_cap) {

	return 0;
}

/* Return the server side acap id for the current upcall thread. */
int acap_srv_lookup(int spdid) {
	int thd_id = cos_get_thd_id();

	/* the parent thread could be preempted (before setting
	 * srv_acap). If we use sched_block/wakeup, then we need
	 * atomic instructions to avoid race. */
	u64_t s, c;
	
	if (unlikely(thd_info[thd_id].srv_spd_id != spdid)) goto err_spd;

	rdtscll(s);
	while (thd_info[thd_id].srv_acap == 0) {
		rdtscll(c);
		if (c-s > 1<<20) {
			printc("warn: thd %d, spinning in acap mgr waiting for acap\n", thd_id);
			s = c;
		}
	}

	return thd_info[thd_id].srv_acap;
err_spd:
	printc("acap_mgr: upcall thread calling lookup from wrong component %d.\n", spdid);
	return -1;
}

/* Return the acap (value) of the current thread (for the static cap_id) on
 * the client side. */
int acap_cli_lookup(int spdid, int cap_id) {
	int srv_thd_id, acap_id, acap_v, cspd, sspd;
	struct thd_info *srv_thd;
	struct thd_acap *thd_acap;
	struct comp_info *ci = &comp_info[spdid];
	int thd_id = cos_get_thd_id();

	if (unlikely(cap_id > ci->ncaps)) goto err_cap;
	assert(ci->cap);

	cspd = spdid;
	sspd = ci->cap[cap_id].srv_comp;

	if (ci->thd_acap[thd_id] == NULL) 
		ci->thd_acap[thd_id] = malloc(sizeof(struct thd_acap));
	thd_acap = ci->thd_acap[thd_id];

	if (thd_acap->acap[cap_id] > 0) //already exist?
		return thd_acap->acap[cap_id];

	/* TODO: lookup some decision table (set by the policy) to
	 * decide whether create a acap for the current thread and the
	 * destination cpu */
	int cpu = 1;

	/* create acap between cspd and sspd */
	acap_id = cos_async_cap_cntl(COS_ACAP_CREATION, cspd, sspd, 0);
	if (acap_id <= 0) { 
		printc("err: async cap creation failed.");
		goto err; 
	}
	acap_v = acap_id | COS_ASYNC_CAP_FLAG;

	/* create server thd and wire. */
	srv_thd_id = create_thd_curr_prio(cpu);
	srv_thd = &thd_info[srv_thd_id];
	//TODO: avoid race between curr and srv_thd.
	srv_thd->cli_spd_id = cspd;
	srv_thd->srv_spd_id = sspd;

	int ret = alloc_ring(srv_thd_id);
	if (ret < 0) {
		printc("acap_mgr: ring buffer allocation error!\n");
		goto err;
	}

	srv_thd->srv_acap = cos_async_cap_cntl(COS_ACAP_WIRE, cspd, acap_id, srv_thd_id);
	assert(srv_thd->srv_acap);

	thd_acap->acap[cap_id] = acap_v;
	
	return acap_v;
err_cap:
	printc("acap_mgr: thread %d calling lookup for non-existing cap %d in component %d.\n",
	       thd_id, cap_id, spdid);
err:
	return 0;
}

int ainv_init(int spdid) {
	return 0;
}

/* External functions done. */


int setup_async_cap(int cspd, int sspd) 
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
	ncaps = cos_cap_cntl(cos_spd_id(), COS_CAP_GET_SPD_NCAPS, cspd, 0);

	if (ncaps <= 0) {
		comp_info[cspd].ncaps = 0;
		goto done;
	}

	comp_info[cspd].ncaps = ncaps;

	comp_info[cspd].cap = malloc(sizeof(struct static_cap) * (ncaps + 1));
	if (unlikely(comp_info[cspd].cap == NULL)) goto err;

	for (i = 0; i < ncaps; i++) {
		cap = &(comp_info[cspd].cap[i]);
		cap->srv_comp = cos_cap_cntl(cos_spd_id(), COS_CAP_GET_DEST_SPD, cspd, i);
		assert(cap->srv_comp);
		cap->shared_acap = 0;
	}
done:
	return 0;
err:
	printc("acap_mgr: cannot allocate memory for acap mapping structure.");
	return -1;
}

static inline int init_cap_info (void) 
{
	int i, ret;

	for (i = 0; i < MAX_NUM_SPDS; i++) {
		ret = init_comp(i);
		if (ret < 0) return ret;
	}

	return 0;
}

void cos_init(void) 
{
	static int first = 1;

	if (first) {
		first = 0;
		printc("thd %d, core %ld in acap mgr\n", cos_get_thd_id(), cos_cpuid());
		init_cap_info();

		int cspd = PING_SPDID, sspd = PONG_SPDID;

		setup_async_cap(cspd, sspd);

		//cos_async_cap_cntl(COS_EVT_WIRE, eid, cos_get_thd_id(), 1);
		sched_wakeup(cos_spd_id(), 19);
		return;
	}

	
	return;
}
