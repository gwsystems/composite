/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <sinv_async.h>

#include <cos_defkernel_api.h>
#include <../interface/sched/sched.h>
#include <../interface/capmgr/capmgr.h>
#include <../interface/channel/channel.h>
#include <res_spec.h>
#include <cos_time.h>

#define SINV_SRV_POLL_US 1000
#define SINV_MAIN_POLL_US 10000

#define SINV_THD_PRIO 2
#define SINV_THD_PERIOD_US 10000
#define SINV_THD_BUDGET_US 4000

static struct cos_aep_info sinv_aeps[MAX_NUM_THREADS];
static int sinv_free_aep = 0;

static void
sinv_server_try_map(struct sinv_async_info *s)
{
	cbuf_t id;
	vaddr_t addr = 0;
	unsigned long npg = 0;

	id = channel_shared_page_map(s->init_key, &addr, &npg);
	if (!id) return;
	assert(id && addr && npg == SINV_INIT_NPAGES);

	s->init_shmaddr = addr;
}

void
sinv_server_init(struct sinv_async_info *s, cos_channelkey_t shm_key)
{
	memset(s, 0, sizeof(struct sinv_async_info));

	s->init_key = shm_key;
	sinv_server_try_map(s);
}

int
sinv_server_api_init(struct sinv_async_info *s, sinv_num_t num, sinv_fn_t fn, sinv_rets_fn_t fnr)
{
	if (num < 0 || num >= SINV_NUM_MAX) return -EINVAL;
	if (!fn && !fnr) return -EINVAL;
	if (fn && fnr) return -EINVAL; /* only one fn ptr should be set */

	if (s->sdata.f[num].sfn) return -EEXIST;

	if (fn) {
		s->sdata.f[num].type = SINV_FN;
		s->sdata.f[num].sfn  = fn;
	} else {
		s->sdata.f[num].type = SINV_RETS_FN;
		s->sdata.f[num].sfnr = fnr;
	}

	return 0;
}

/* for default functionality, inv functions must be initialized */
CWEAKSYMB int
sinv_server_entry(struct sinv_async_info *s, struct sinv_call_req *req)
{
	int ret = 0;

	assert(s);
	assert(req);
	assert(req->callno >= 0 && req->callno < SINV_NUM_MAX);

	switch(s->sdata.f[req->callno].type) {
	case SINV_RETS_FN:
	{
		sinv_rets_fn_t fnr = s->sdata.f[req->callno].sfnr;

		assert(fnr);
		ret = (fnr)(&(req->ret2), &(req->ret3), req->arg1, req->arg2, req->arg3);
		break;
	}
	case SINV_FN:
	{
		sinv_fn_t fn = s->sdata.f[req->callno].sfn;

		assert(fn);
		ret = (fn)(req->arg1, req->arg2, req->arg3);
		break;
	}
	default: assert(0);
	}

	return ret;
}

static void
sinv_server_aep_fn(arcvcap_t rcv, void *data)
{
	struct sinv_async_info *s = (struct sinv_async_info *)data;
	struct sinv_thdinfo    *t = NULL;

	assert(s);
	t = &s->sdata.sthds[cos_thdid()];

	while (1) {
		volatile unsigned long *reqaddr = (volatile unsigned long *)SINV_POLL_ADDR(t->shmaddr);
		asndcap_t snd = t->sndcap;
		int *retval = (int *)SINV_RET_ADDR(t->shmaddr), ret;
		struct sinv_call_req *req = (struct sinv_call_req *)SINV_REQ_ADDR(t->shmaddr);
		int rcvd = 0;

		while ((cos_rcv(rcv, RCV_NON_BLOCKING | RCV_ALL_PENDING, &rcvd) < 0)) {
			cycles_t timeout = time_now() + time_usec2cyc(SINV_SRV_POLL_US);

			if (ps_load((unsigned long *)reqaddr) == SINV_REQ_SET) break;
			sched_thd_block_timeout(0, timeout);
		}

		assert(ps_load((unsigned long *)reqaddr) == SINV_REQ_SET);
		*retval = sinv_server_entry(s, req);

		ret = ps_cas((unsigned long *)reqaddr, SINV_REQ_SET, SINV_REQ_RESET); /* indicate request completion */
		assert(ret);

		/* if (snd) cos_asnd(snd, 1); */
	}
}

/*
 * Requests are synchronous. Client "scheduler" should make sure it synchronously calls this and the calling path is atomic!!.
 * Request on SHM:
 *  unsigned long req_flag, client set it to 1 & server reset it to 0.
 *  int returnval, set by the server to indicate the return value of a request.
 *  struct sinv_thdcrt_req with client key and server key:
 *        spdid_t of the client..
 *        cos_channelkey_t clientkey, key for "asnd" to client on completion.
 *        cos_channelkey_t serverkey, key for shared memory (created by client) and used by aep creation on server side. client should create "asndcap" after return from a request.
 */
int
sinv_server_main_loop(struct sinv_async_info *s)
{
	while (!s->init_shmaddr) sinv_server_try_map(s);

	while (1) {
		volatile unsigned long *reqaddr = (volatile unsigned long *)SINV_POLL_ADDR(s->init_shmaddr);
		int *retval = (int *)SINV_RET_ADDR(s->init_shmaddr), ret;
		struct sinv_thdcrt_req *req = (struct sinv_thdcrt_req *)SINV_REQ_ADDR(s->init_shmaddr);
		int aep_slot = (int)ps_faa((unsigned long *)&sinv_free_aep, 1);
		struct cos_aep_info *aep = NULL;
		thdid_t tid = 0;
		asndcap_t snd = 0;
		cbuf_t id = 0;
		vaddr_t shmaddr = 0;
		unsigned long npages = 0;

		assert(aep_slot < MAX_NUM_THREADS);
		aep = &sinv_aeps[aep_slot];
		memset(aep, 0, sizeof(struct cos_aep_info));

		while (ps_load((unsigned long *)reqaddr) != SINV_REQ_SET) {
			cycles_t timeout = time_now() + time_usec2cyc(SINV_MAIN_POLL_US);

			sched_thd_block_timeout(0, timeout);
		}

		assert(req->skey);
		tid = sched_aep_create(aep, sinv_server_aep_fn, (void *)s, 0, req->skey, 0, 0);
		assert(tid);

		id = channel_shared_page_map(req->skey, &shmaddr, &npages);
		assert(id && shmaddr && npages == SINV_REQ_NPAGES);

		if (req->rkey) {
			snd = capmgr_asnd_key_create(req->rkey);
			assert(snd);

			s->sdata.sthds[tid].rkey = req->rkey;
		}

		s->sdata.sthds[tid].rcvcap   = aep->rcv;
		s->sdata.sthds[tid].skey     = req->skey;
		s->sdata.sthds[tid].sndcap   = snd;
		s->sdata.sthds[tid].shmaddr  = shmaddr;
		s->sdata.sthds[tid].clientid = req->clspdid;
		sched_thd_param_set(tid, sched_param_pack(SCHEDP_WINDOW, SINV_THD_PERIOD_US));
		sched_thd_param_set(tid, sched_param_pack(SCHEDP_BUDGET, SINV_THD_BUDGET_US));
		sched_thd_param_set(tid, sched_param_pack(SCHEDP_PRIO, SINV_THD_PRIO));

		*retval = 0;
		ret = ps_cas((unsigned long *)reqaddr, SINV_REQ_SET, SINV_REQ_RESET); /* indicate request completion */
		assert(ret);
	}
}
