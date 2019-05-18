#include <ps.h>
#include <ck_ring.h>
#include <sl_xcore.h>
#include <sl.h>
#include <bitmap.h>

/******************************* Client-side ***************************/

/* static xcore thread backend! mainly for bookkeeping across cores! */
static struct sl_xcore_thd _xcore_thds[MAX_NUM_THREADS];
extern void sl_thd_param_set_no_cs(struct sl_thd *, sched_param_t);

static inline void
_sl_xcore_response_wait(struct sl_xcore_response *r)
{
	if (sl_thd_curr() != sl__globals_core()->sched_thd) {
		if (!ps_load(&r->resp_ready)) sl_thd_block(0);
	} else {
		while (!ps_load(&r->resp_ready)) {
			if (sl_cs_enter_sched()) continue;
			sl_cs_exit_schedule_nospin();
		}
	}
	assert(r->resp_ready);
}

static inline struct sl_xcore_thd *
_sl_xcore_thd_backend_lookup(thdid_t tid)
{
	return &_xcore_thds[tid];
}

static inline struct sl_xcore_thd *
_sl_xcore_thd_backend_init(thdid_t tid, cpuid_t core, asndcap_t snd)
{
	struct sl_xcore_thd *t = _sl_xcore_thd_backend_lookup(tid);

	if (unlikely(t->thd)) return t;
	t->thd  = tid;
	t->core = core;

	return t;
}

struct sl_xcore_thd *
sl_xcore_thd_lookup_init(thdid_t tid, cpuid_t core)
{
	struct sl_xcore_thd *t = _sl_xcore_thd_backend_lookup(tid);

	/* TODO: is this safe? a wrong coreid can cause DOS! */
	if (unlikely(!(t->thd))) return _sl_xcore_thd_backend_init(tid, core, 0);

	/* perhaps migrated! */
	if (unlikely(t->core != core)) t->core = core;
	/* if (unlikely(t->core != core)) return NULL; */

	return t;
}

struct sl_xcore_thd *
sl_xcore_thd_lookup(thdid_t tid)
{
	return _sl_xcore_thd_backend_lookup(tid);
}

#define SL_XCORE_REQ(req, typ, resp) do { 		\
			req.type        = typ;		\
			req.client_core = cos_cpuid();	\
			req.client_thd  = cos_thdid();	\
			req.response    = resp;		\
		} while (0)

#define SL_XCORE_RESP(resp, typ) do {			\
			resp.type       = typ;		\
			resp.resp_ready = 0;		\
		} while (0)

extern struct sl_thd *sl_thd_alloc_no_cs(cos_thd_fn_t fn, void *data);

static inline int
_sl_xcore_request_enqueue_no_cs(cpuid_t core, struct sl_xcore_request *rq)
{
	int ret = 0;
	asndcap_t snd = 0;
	
	if (unlikely(core >= NUM_CPU)) return -1;
	if (unlikely(core == cos_cpuid())) return -1;
	if (unlikely(!bitmap_check(sl__globals()->core_bmp, core))) return -1;
	ret = ck_ring_enqueue_mpsc_xcore(sl__ring(core), sl__ring_buffer(core), rq);
	snd = sl__globals()->xcore_asnd[cos_cpuid()][core];
	assert(snd);

//	/* send an IPI for the request */
	cos_asnd(snd, 0);

	if (unlikely(ret == false)) return -1;

	return 0;
}

static inline int
_sl_xcore_request_enqueue(cpuid_t core, struct sl_xcore_request *rq)
{
	int ret = 0;
	
	if (unlikely(core >= NUM_CPU)) return -1;
	sl_cs_enter();
	ret = _sl_xcore_request_enqueue_no_cs(core, rq);
	sl_cs_exit();
	if (unlikely(ret)) return -1;


	return 0;
}

struct sl_xcore_thd *
sl_xcore_thd_alloc(cpuid_t core, cos_thd_fn_t fn, void *data, int nparams, sched_param_t params[])
{
	int ret = 0;
	asndcap_t snd = 0;
	struct sl_xcore_request req;
	struct sl_xcore_response resp;
	thdid_t xcore_tid;

	SL_XCORE_REQ(req, SL_XCORE_THD_ALLOC, &resp);
	SL_XCORE_RESP(resp, SL_XCORE_THD_ALLOC);
	req.sl_xcore_req_thd_alloc.fn = fn;
	req.sl_xcore_req_thd_alloc.data = data;
	if (nparams) memcpy(req.sl_xcore_req_thd_alloc.params, params, sizeof(sched_param_t) * nparams);
	req.sl_xcore_req_thd_alloc.param_count = nparams;

	ret = _sl_xcore_request_enqueue(core, &req);
	if (unlikely(ret)) return NULL;

	/* Other core will wake this up after creation! */
	_sl_xcore_response_wait(&resp);
	xcore_tid = resp.sl_xcore_resp_thd_alloc.tid;
	assert(xcore_tid);
	
	return _sl_xcore_thd_backend_init(xcore_tid, core, 0);
}

struct sl_xcore_thd *
sl_xcore_thd_alloc_ext(cpuid_t core, struct cos_defcompinfo *dci, thdclosure_index_t idx, int nparams, sched_param_t params[])
{
	return NULL;
}

struct sl_xcore_thd *
sl_xcore_aep_alloc(cpuid_t core, cos_thd_fn_t fn, void *data, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[])
{
	return NULL;
}

struct sl_xcore_thd *
sl_xcore_aep_alloc_ext(cpuid_t core, struct cos_defcompinfo *dci, thdclosure_index_t idx, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[])
{
	return NULL;
}

struct sl_xcore_thd *
sl_xcore_initaep_alloc(cpuid_t core, struct cos_defcompinfo *dci, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[])
{
	return NULL;
}

struct sl_xcore_thd *
sl_xcore_initaep_alloc_ext(cpuid_t core, struct cos_defcompinfo *dci, struct cos_defcompinfo *sched, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[])
{
	return NULL;
}

void
sl_xcore_thd_param_set(struct sl_xcore_thd *t, sched_param_t param)
{
	struct sl_xcore_request req;
	cpuid_t core = sl_xcore_thd_core(t);

	SL_XCORE_REQ(req, SL_XCORE_THD_PARAM_SET, 0);
	req.sl_xcore_req_thd_param_set.tid   = sl_xcore_thd_thdid(t);
	req.sl_xcore_req_thd_param_set.param = param;

	_sl_xcore_request_enqueue(core, &req);
}

static inline void
_sl_xcore_thd_wakeup_tid_no_cs(thdid_t tid, cpuid_t core)
{
	struct sl_xcore_request req;

	SL_XCORE_REQ(req, SL_XCORE_THD_WAKEUP, 0);
	req.sl_xcore_req_thd_wakeup.tid = tid;
	_sl_xcore_request_enqueue_no_cs(core, &req);
}

void
sl_xcore_thd_wakeup(struct sl_xcore_thd *t)
{
	struct sl_xcore_request req;
	cpuid_t core = sl_xcore_thd_core(t);

	if (unlikely(!t)) return;

	SL_XCORE_REQ(req, SL_XCORE_THD_WAKEUP, 0);
	req.sl_xcore_req_thd_wakeup.tid = sl_xcore_thd_thdid(t);
	_sl_xcore_request_enqueue(core, &req);
}

void
sl_xcore_thd_wakeup_tid(thdid_t tid, cpuid_t core)
{
	struct sl_xcore_thd *t = sl_xcore_thd_lookup(tid);

	assert(t->core == core);

	sl_xcore_thd_wakeup(t);
}

int
sl_xcore_load_balance(void)
{
	struct sl_xcore_request req;
	struct sl_xcore_response resp;
	struct sl_global *g = sl__globals();
	unsigned max = 0, i, nthds = 0;
	int core = -1, ret;

	for (i = 0; i < NUM_CPU; i++) {
		if (!bitmap_check(g->core_bmp, i)) continue;

		if (g->nthds_running[i] <= max) continue;

		max = g->nthds_running[i];
		core = i;
		break;
	}

	if (max == 0 || core == -1) return -1;

	memset(&req, 0, sizeof(req));
	SL_XCORE_REQ(req, SL_XCORE_LOAD_BALANCE, &resp);
	SL_XCORE_RESP(resp, SL_XCORE_LOAD_BALANCE);
	req.sl_xcore_req_load_balance.nthds = 1; /* FIXME: lets start with just 1 */
	ret = _sl_xcore_request_enqueue((cpuid_t)core, &req);
	if (unlikely(ret)) return -1;

	_sl_xcore_response_wait(&resp);
	nthds = resp.sl_xcore_resp_load_balance.nthds;
	if (!nthds) return 0;

	assert(nthds < SL_XCORE_MIGRATE_MAX);
	sl_cs_enter();
	for (i = 0; i < nthds; i++) {
		struct sl_thd *t = sl_thd_lkup(resp.sl_xcore_resp_load_balance.tid[i]);

		assert(t);
		assert(t->state == SL_THD_RUNNABLE);
		sl_mod_wakeup(sl_mod_thd_policy_get(t));
		ps_faa(&(g->nthds_running[cos_cpuid()]), 1);
	}
	sl_cs_exit();

	return nthds;
}

/******************************* Server-side ***************************/
static inline void
_sl_xcore_respond(struct sl_xcore_request *req)
{
	struct sl_xcore_response *resp = req->response;

	if (!resp) return;

	assert(resp->type == req->type && ps_load(&resp->resp_ready) == 0);
	ps_faa(&resp->resp_ready, 1);
	_sl_xcore_thd_wakeup_tid_no_cs(req->client_thd, req->client_core);
}

static inline int
_sl_xcore_req_thd_alloc_no_cs(struct sl_xcore_request *req)
{
	cos_thd_fn_t   fn   = req->sl_xcore_req_thd_alloc.fn;
	void          *data = req->sl_xcore_req_thd_alloc.data;
	struct sl_thd *t;
	struct sl_xcore_response *x = req->response;
	int i;

	assert(fn);

	t = sl_thd_alloc_no_cs(fn, data);
	assert(t);
	if (likely(x)) x->sl_xcore_resp_thd_alloc.tid = sl_thd_thdid(t);
	for (i = 0; i < req->sl_xcore_req_thd_alloc.param_count; i++) sl_thd_param_set_no_cs(t, req->sl_xcore_req_thd_alloc.params[i]);

	return 0;
}

static inline int
_sl_xcore_req_thd_param_set_no_cs(struct sl_xcore_request *req)
{
	struct sl_thd *t = sl_thd_lkup(req->sl_xcore_req_thd_param_set.tid);

	if (!t) return -1;
	sl_thd_param_set_no_cs(t, req->sl_xcore_req_thd_param_set.param);

	return 0;
}

static inline int
_sl_xcore_req_thd_wakeup_no_cs(struct sl_xcore_request *req)
{
	struct sl_thd *t = sl_thd_lkup(req->sl_xcore_req_thd_param_set.tid);

	if (!t) return -1;
	if (unlikely(t == sl__globals_core()->sched_thd)) return 0;
	sl_thd_wakeup_no_cs(t);

	return 0;
}

static inline void 
_sl_xcore_req_load_balance_no_cs(struct sl_xcore_request *req)
{
	struct sl_global *g = sl__globals();
	int n = g->nthds_running[cos_cpuid()], i, j = 0;
	struct sl_xcore_response *rp = req->response;
	cpuid_t cl_core = req->client_core;

	if (n <= SL_XCORE_KEEP_MIN) return;
	n -= SL_XCORE_KEEP_MIN;

	if (n > SL_XCORE_MIGRATE_MAX) n = SL_XCORE_MIGRATE_MAX;
	if (n > req->sl_xcore_req_load_balance.nthds) n = req->sl_xcore_req_load_balance.nthds;

	assert(rp);
	for (i = 0; i < n; i++) {
		struct sl_thd_policy *t = sl_mod_last_schedule();
		thdid_t tid = 0;
		struct sl_xcore_thd *xt = NULL;

		if (!t) break;
		tid = sl_thd_thdid(sl_mod_thd_get(t));
		xt = sl_xcore_thd_lookup(tid);
		assert(xt);
		if (xt->thd == tid) assert(xt->core == cos_cpuid());
		if (sl_thd_migrate_no_cs(sl_mod_thd_get(t), cl_core)) break;
		sl_xcore_thd_lookup_init(tid, cl_core);
		rp->sl_xcore_resp_load_balance.tid[i] = tid;
	}
	rp->sl_xcore_resp_load_balance.nthds = i;

	return;
}

int
sl_xcore_process_no_cs(void)
{
	int num = 0;
	struct sl_xcore_request xcore_req;

	if (likely(NUM_CPU < 2)) return 0;

	while (ck_ring_dequeue_mpsc_xcore(sl__ring_curr(), sl__ring_buffer_curr(), &xcore_req) == true) {
		assert(xcore_req.client_core != cos_cpuid());

		switch(xcore_req.type) {
		case SL_XCORE_THD_ALLOC:
		{
			_sl_xcore_req_thd_alloc_no_cs(&xcore_req);
			break;
		}
		case SL_XCORE_THD_ALLOC_EXT:
		case SL_XCORE_AEP_ALLOC:
		case SL_XCORE_AEP_ALLOC_EXT:
		case SL_XCORE_INITAEP_ALLOC:
		case SL_XCORE_THD_DEALLOC:
		{
			PRINTC("Unimplemented request! Aborting!\n");
			assert(0);

			break;
		}
		case SL_XCORE_THD_PARAM_SET:
		{
			_sl_xcore_req_thd_param_set_no_cs(&xcore_req);
			break;
		}
		case SL_XCORE_THD_WAKEUP:
		{
			_sl_xcore_req_thd_wakeup_no_cs(&xcore_req);
			break;
		}
		case SL_XCORE_LOAD_BALANCE:
		{
			_sl_xcore_req_load_balance_no_cs(&xcore_req);
			break;
		}
		default:
		{
			PRINTC("Unrecognized request! Aborting!\n");
			assert(0);
		}
		}
		_sl_xcore_respond(&xcore_req);
		num ++;
	}

	return num; /* number of requests processed */
}
