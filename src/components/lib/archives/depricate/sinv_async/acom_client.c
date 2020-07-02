/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <sinv_async.h>

#include <cos_time.h>
#include <../interface/sched/sched.h>
#include <../interface/capmgr/capmgr.h>
#include <../interface/channel/channel.h>

#define SINV_SRV_POLL_US 1000

void
acom_client_init(struct sinv_async_info *s, cos_channelkey_t shm_key)
{
	cbuf_t id;
	vaddr_t addr = 0;

	memset(s, 0, sizeof(struct sinv_async_info));

	id = channel_shared_page_allocn(shm_key, SINV_INIT_NPAGES, &addr);
	assert(id && addr);

	s->init_key     = shm_key;
	s->init_shmaddr = addr;
}

int
acom_client_thread_init(struct sinv_async_info *s, thdid_t tid, arcvcap_t rcv, cos_channelkey_t rcvkey, cos_channelkey_t skey)
{
	volatile unsigned long *reqaddr = (volatile unsigned long *)SINV_POLL_ADDR(s->init_shmaddr);
	int *retval = (int *)SINV_RET_ADDR(s->init_shmaddr), ret;
	struct sinv_thdcrt_req *req = (struct sinv_thdcrt_req *)SINV_REQ_ADDR(s->init_shmaddr);
	struct sinv_thdinfo *tinfo = &s->cdata.cthds[tid];
	vaddr_t shmaddr = 0;
	cbuf_t id = 0;
	asndcap_t snd = 0;
	spdid_t child = cos_inv_token() == 0 ? cos_spd_id() : (spdid_t)cos_inv_token();

	assert(ps_load((unsigned long *)reqaddr) == SINV_REQ_RESET);
	assert(rcvkey && skey && tid && rcv);

	req->clspdid = child; /* this is done from the scheduler on invocation */
	req->rkey = rcvkey;
	req->skey = skey;

	id = channel_shared_page_allocn(skey, SINV_REQ_NPAGES, &shmaddr);
	assert(id && shmaddr);

	ret = ps_cas((unsigned long *)reqaddr, SINV_REQ_RESET, SINV_REQ_SET); /* indicate request available */
	assert(ret);

	while (ps_load((unsigned long *)reqaddr) != SINV_REQ_RESET) {
		cycles_t timeout = time_now() + time_usec2cyc(SINV_SRV_POLL_US);

		sched_thd_block_timeout(0, timeout); /* called from the scheduler */
	}

	/* TODO: UNDO!!! */
	if (*retval) return *retval;

	snd = capmgr_asnd_key_create(skey);
	assert(snd);

	tinfo->rkey     = rcvkey;
	tinfo->skey     = skey;
	tinfo->clientid = child;
	tinfo->sndcap   = snd;
	tinfo->rcvcap   = rcv; /* AEP thread, so rcv with it's rcvcap */
	tinfo->shmaddr  = shmaddr;

	return 0;
}

int
acom_client_request(struct sinv_async_info *s, acom_type_t t, word_t a, word_t b, word_t c, tcap_res_t budget, tcap_prio_t prio)
{
	struct sinv_thdinfo *tinfo = &s->cdata.cthds[cos_thdid()];
	volatile unsigned long *reqaddr = (volatile unsigned long *)SINV_POLL_ADDR(tinfo->shmaddr);
	int *retval = NULL, ret, rcvd = 0;
	struct sinv_call_req *req = NULL;

	assert(t >= 0 && t < SINV_NUM_MAX);
	assert(reqaddr);
	assert(tinfo->rcvcap);

	retval = (int *)SINV_RET_ADDR(tinfo->shmaddr);
	req    = (struct sinv_call_req *)SINV_REQ_ADDR(tinfo->shmaddr);

	req->callno = t;
	req->arg1   = a;
	req->arg2   = b;
	req->arg3   = c;

	ret = ps_cas((unsigned long *)reqaddr, SINV_REQ_RESET, SINV_REQ_SET);
	assert(ret); /* must be sync.. */

	assert(tinfo->sndcap);
	if (budget) {
		/* TODO: scheduler API for delegation, apps don't have access to "Tcap" */
	} else {
		/* cos_asnd(tinfo->sndcap, 0); */
	}
	cos_asnd(tinfo->sndcap, 1);

	assert(tinfo->rcvcap);
	while ((cos_rcv(tinfo->rcvcap, RCV_NON_BLOCKING | RCV_ALL_PENDING, &rcvd) < 0)) {
		cycles_t timeout = time_now() + time_usec2cyc(SINV_SRV_POLL_US);

		if (ps_load((unsigned long *)reqaddr) == SINV_REQ_RESET) break;

		sched_thd_block_timeout(0, timeout); /* in the app component */

		/*
		 * Though this is synchronous, we could bound this by having a kind of
		 * inbuilt watchdog timer that triggers and returns perhaps -ETIMEDOUT
		 * if a low-assurance component doesn't respond before the watchdog timer
		 * triggers.
		 *
		 * In such a case, user cannot (should not) make any further requests.
		 * (simplicity) because, we cannot be sure if the server is still processing
		 * the previous requests or not and overwriting will just break the server!!
		 */
	}

	assert(ps_load((unsigned long *)reqaddr) == SINV_REQ_RESET);

	return *retval;
}
