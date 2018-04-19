#include <sinv_async.h>

#include <sl.h>
#include <../interface/sched/sched.h>
#include <../interface/capmgr/capmgr.h>
#include <../interface/channel/channel.h>

#define SINV_SRV_POLL_US 500
#define USEC_2_CYC 2800 /* TODO: move out some generic parts from sl */

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

	if (fn) s->sdata.f[num].sfn  = fn;
	else    s->sdata.f[num].sfnr = fnr;

	return 0;
}

void
sinv_server_fn(arcvcap_t rcv, void *data)
{
	struct sinv_async_info *s = (struct sinv_async_info *)data;

	while (1) {
		unsigned long *reqaddr = (unsigned long *)(s->sdata.sthds[cos_thdid()].shmaddr);
		asndcap_t snd = s->sdata.sthds[cos_thdid()].sndcap;
		int *retval = (int *)(reqaddr + 1), ret;
		struct sinv_call_req *req = (struct sinv_call_req *)(reqaddr + 2);
		sinv_fn_t fn;
		sinv_rets_fn_t fnr;

		while (ps_load(reqaddr) != 1 || cos_rcv(rcv, RCV_NON_BLOCKING, NULL) < 0) {
			cycles_t now, timeout;

			rdtscll(now);
			timeout = now + (SINV_SRV_POLL_US * USEC_2_CYC);
			sched_thd_block_timeout(0, timeout);
		}

		assert(ps_load(reqaddr) == 1);
		assert(req->callno >= 0 && req->callno < SINV_NUM_MAX);

		/* TODO: switch case here for interface specific calling convention */
		switch(req->callno) {
		case 0: /* FIXME: just a test */
		{
			fnr = s->sdata.f[req->callno].sfnr;
			assert(fnr);
			*retval = (fnr)(&(req->ret2), &(req->ret3), req->arg1, req->arg2, req->arg3);
			break;
		}
		default:
		{
			fn  = s->sdata.f[req->callno].sfn;
			assert(fn);
			*retval = (fn)(req->arg1, req->arg2, req->arg3);
		}
		}
		ret = ps_cas(reqaddr, 1, 0); /* indicate request completion */
		assert(ret);

		/* TODO: asnd using capmgr interface for rate-limiting? */
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
		unsigned long *reqaddr = (unsigned long *)(s->init_shmaddr);
		int *retval = (int *)(reqaddr + 1), ret;
		struct sinv_thdcrt_req *req = (struct sinv_thdcrt_req *)(reqaddr + 2);
		struct cos_aep_info aep;
		thdid_t tid = 0;
		asndcap_t snd = 0;
		cbuf_t id = 0;
		vaddr_t shmaddr = 0;
		unsigned long npages = 0;

		memset(&aep, 0, sizeof(struct cos_aep_info));
		/* TODO: cos_rcv! */
		while (ps_load(reqaddr) != 1) {
			cycles_t now, timeout;

			rdtscll(now);
			timeout = now + (SINV_SRV_POLL_US * USEC_2_CYC);
			sched_thd_block_timeout(0, timeout);
		}

		assert(req->skey);
		tid = sched_aep_create(&aep, sinv_server_fn, (void *)s, 1, req->skey, 0, 0);
		assert(tid);

		id = channel_shared_page_map(req->skey, &shmaddr, &npages);
		assert(id && shmaddr && npages == SINV_REQ_NPAGES);

		if (req->rkey) {
			snd = capmgr_asnd_key_create(req->rkey);
			assert(snd);

			s->sdata.sthds[tid].rkey = req->rkey;
		}

		s->sdata.sthds[tid].rcvcap   = aep.rcv;
		s->sdata.sthds[tid].skey     = req->skey;
		s->sdata.sthds[tid].sndcap   = snd;
		s->sdata.sthds[tid].shmaddr  = shmaddr;
		s->sdata.sthds[tid].clientid = req->clspdid;

		*retval = 0;
		ret = ps_cas(reqaddr, 1, 0); /* indicate request completion */
		assert(ret);
		/* TODO: cos_asnd? */
	}
}

void
sinv_client_init(struct sinv_async_info *s, cos_channelkey_t shm_key)
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
sinv_client_thread_init(struct sinv_async_info *s, thdid_t tid, cos_channelkey_t rcvkey, cos_channelkey_t skey)
{
	unsigned long *reqaddr = (unsigned long *)(s->init_shmaddr);
	int *retval = (int *)(reqaddr + 1), ret;
	struct sinv_thdcrt_req *req = (struct sinv_thdcrt_req *)(reqaddr + 2);
	struct sinv_thdinfo *tinfo = &s->cdata.cthds[tid];
	vaddr_t shmaddr = 0;
	cbuf_t id = 0;
	asndcap_t snd = 0;
	arcvcap_t rcv = 0;
	spdid_t child = cos_inv_token() == 0 ? cos_spd_id() : cos_inv_token();

	assert(ps_load(reqaddr) == 0);

	req->clspdid = child; /* this is done from the scheduler on invocation */
	req->rkey = rcvkey;
	req->skey = skey;

	id = channel_shared_page_allocn(skey, SINV_REQ_NPAGES, &shmaddr);
	assert(id && shmaddr);

	if (rcvkey) {
		/* capmgr interface to create a rcvcap for "tid" thread in the scheduler component..*/
		rcv = capmgr_rcv_create(child, tid, rcvkey, 0, 0); /* TODO: rate- limit */
		assert(rcv);
	}

	ret = ps_cas(reqaddr, 0, 1); /* indicate request available */
	assert(ret);
	/* TODO: cos_asnd */

	/* TODO: cos_rcv! */
	while (ps_load(reqaddr) != 0) {
		cycles_t now, timeout;

		rdtscll(now);
		timeout = now + (SINV_SRV_POLL_US * USEC_2_CYC);
		sl_thd_block_timeout(0, timeout); /* called from the scheduler */
	}

	/* TODO: UNDO!!! */
	if (*retval) return *retval;

	snd = capmgr_asnd_key_create(skey);
	assert(snd);

	tinfo->rkey     = rcvkey;
	tinfo->skey     = skey;
	tinfo->clientid = child;
	tinfo->sndcap   = snd;
	tinfo->rcvcap   = rcv; /* cos_rcv in the scheduler */
	tinfo->shmaddr  = shmaddr;

	return 0;
}

static int
sinv_client_call_wrets(int wrets, struct sinv_async_info *s, sinv_num_t n, word_t a, word_t b, word_t c, word_t *r2, word_t *r3)
{
	struct sinv_thdinfo *tinfo = &s->cdata.cthds[cos_thdid()];
	unsigned long *reqaddr = (unsigned long *)tinfo->shmaddr;
	int *retval = NULL, ret;
	struct sinv_call_req *req = NULL;

	assert(n >= 0 && n < SINV_NUM_MAX);
	assert(reqaddr);

	retval = (int *)(reqaddr + 1);
	req    = (struct sinv_call_req *)(reqaddr + 2);

	req->callno = n;
	req->arg1   = a;
	req->arg2   = b;
	req->arg3   = c;

	ret = ps_cas(reqaddr, 0, 1);
	assert(ret); /* must be sync.. */

	/* TODO: use the scheduler's rate-limiting api */
	/* cos_asnd(tinfo->sndcap, 0); */

	while (ps_load(reqaddr) != 0 || !tinfo->rcvcap || cos_rcv(tinfo->rcvcap, RCV_NON_BLOCKING, NULL) < 0) {
		cycles_t now, timeout;

		rdtscll(now);
		timeout = now + (SINV_SRV_POLL_US * USEC_2_CYC);
		sl_thd_block_timeout(0, timeout); /* in the scheduler component */
	}

	assert(ps_load(reqaddr) == 0);
	if (!wrets) goto done;

	*r2 = req->ret2;
	*r3 = req->ret3;

done:
	return *retval;
}

int
sinv_client_call(struct sinv_async_info *s, sinv_num_t n, word_t a, word_t b, word_t c)
{
	return sinv_client_call_wrets(0, s, n, a, b, c, NULL, NULL);
}

int
sinv_client_rets_call(struct sinv_async_info *s, sinv_num_t n, word_t *r2, word_t *r3, word_t a, word_t b, word_t c)
{
	return sinv_client_call_wrets(1, s, n, a, b, c, r2, r3);
}

int
acom_client_thread_init(struct sinv_async_info *s, thdid_t tid, arcvcap_t rcv, cos_channelkey_t rcvkey, cos_channelkey_t skey)
{
	unsigned long *reqaddr = (unsigned long *)(s->init_shmaddr);
	int *retval = (int *)(reqaddr + 1), ret;
	struct sinv_thdcrt_req *req = (struct sinv_thdcrt_req *)(reqaddr + 2);
	struct sinv_thdinfo *tinfo = &s->cdata.cthds[tid];
	vaddr_t shmaddr = 0;
	cbuf_t id = 0;
	asndcap_t snd = 0;
	spdid_t child = cos_inv_token() == 0 ? cos_spd_id() : cos_inv_token();

	assert(ps_load(reqaddr) == 0);
	assert(rcvkey && skey && tid && rcv);

	req->clspdid = child; /* this is done from the scheduler on invocation */
	req->rkey = rcvkey;
	req->skey = skey;

	id = channel_shared_page_allocn(skey, SINV_REQ_NPAGES, &shmaddr);
	assert(id && shmaddr);

	ret = ps_cas(reqaddr, 0, 1); /* indicate request available */
	assert(ret);
	/* TODO: cos_asnd */

	/* TODO: cos_rcv! */
	while (ps_load(reqaddr) != 0) {
		cycles_t now, timeout;

		rdtscll(now);
		timeout = now + (SINV_SRV_POLL_US * USEC_2_CYC);
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

static int
acomm_client_request(struct sinv_async_info *s, acom_type_t t, word_t a, word_t b, word_t c, tcap_res_t budget, tcap_prio_t prio)
{
	struct sinv_thdinfo *tinfo = &s->cdata.cthds[cos_thdid()];
	unsigned long *reqaddr = (unsigned long *)tinfo->shmaddr;
	int *retval = NULL, ret;
	struct sinv_call_req *req = NULL;

	assert(t >= 0 && t < SINV_NUM_MAX);
	assert(reqaddr);
	assert(tinfo->rcvcap);

	retval = (int *)(reqaddr + 1);
	req    = (struct sinv_call_req *)(reqaddr + 2);

	req->callno = t;
	req->arg1   = a;
	req->arg2   = b;
	req->arg3   = c;

	ret = ps_cas(reqaddr, 0, 1);
	assert(ret); /* must be sync.. */

	if (budget) {
		/* scheduler API for delegation, apps don't have access to "Tcap" */
	} else {
		/* cos_asnd(tinfo->sndcap, 0); */
	}

	while (ps_load(reqaddr) != 0 || cos_rcv(tinfo->rcvcap, RCV_NON_BLOCKING, NULL) < 0) {
		cycles_t now, timeout;

		rdtscll(now);
		timeout = now + (SINV_SRV_POLL_US * USEC_2_CYC);
		sched_thd_block_timeout(0, timeout); /* in the app component */
	}

	assert(ps_load(reqaddr) == 0);

	return *retval;
}
