#include <ps.h>
#include <ck_ring.h>
#include <sl_xcore.h>
#include <sl.h>
#include <bitmap.h>

/******************************* Client-side ***************************/

/* static xcore thread backend! mainly for bookkeeping across cores! */
struct _sl_xcore_thds {
	struct sl_xcore_thd _thds[MAX_NUM_THREADS];
} CACHE_ALIGNED;

static struct _sl_xcore_thds _xcore_thds[NUM_CPU];

static inline struct sl_xcore_thd *
_sl_xcore_thd_backend_lookup(thdid_t tid)
{
	return &(_xcore_thds[cos_cpuid()]._thds[tid]);
}

static inline struct sl_xcore_thd *
_sl_xcore_thd_backend_init(thdid_t tid, cpuid_t core, asndcap_t snd)
{
	struct sl_xcore_thd *t = _sl_xcore_thd_backend_lookup(tid);

	sl_cs_enter();
	if (unlikely(t->thd)) goto done;
	t->thd  = tid;
	t->core = core;
	t->asnd = snd;

done:
	sl_cs_exit();

	return t;
}

struct sl_xcore_thd *
sl_xcore_thd_lookup(thdid_t tid, cpuid_t core)
{
	struct sl_xcore_thd *t = _sl_xcore_thd_backend_lookup(tid);

	/* TODO: is this safe? a wrong coreid can cause DOS! */
	if (unlikely(!(t->thd))) return _sl_xcore_thd_backend_init(tid, core, 0);
	/* something wrong! */
	if (unlikely(t->core != core)) return NULL;

	return t;
}

#define SL_XCORE_REQ(req, typ, resp) do { 				\
					req.type        = typ;		\
					req.client_core = cos_cpuid();	\
					req.client_thd  = cos_thdid();	\
					req.response    = resp;		\
					} while (0)

extern struct sl_thd *sl_thd_alloc_no_cs(cos_thd_fn_t fn, void *data);

static inline int
_sl_xcore_request_enqueue_no_cs(cpuid_t core, struct sl_xcore_request *rq)
{
	int ret = 0;
	
	if (unlikely(core >= NUM_CPU)) return -1;
	if (unlikely(core == cos_cpuid())) return -1;
	if (unlikely(!bitmap_check(sl__globals()->core_bmp, core))) return -1;
	ret = ck_ring_enqueue_mpsc_xcore(sl__ring(core), sl__ring_buffer(core), rq);
	if (unlikely(ret == false)) return -1;

	return 0;
}

static inline int
_sl_xcore_request_enqueue(cpuid_t core, struct sl_xcore_request *rq)
{
	int ret = 0;
	/* asndcap_t snd = 0; */
	
	if (unlikely(core >= NUM_CPU)) return -1;
	sl_cs_enter();
	ret = _sl_xcore_request_enqueue_no_cs(core, rq);
	sl_cs_exit();
	if (unlikely(ret)) return -1;

	/* snd = sl__globals()->xcore_asnd[cos_cpuid()][core]; */
	/* assert(snd); */

	/* send an IPI for the request */
	/* if (snd) cos_asnd(snd, 0); */

	return 0;
}

struct sl_xcore_thd *
sl_xcore_thd_alloc(cpuid_t core, cos_thd_fn_t fn, void *data, int nparams, sched_param_t params[])
{
	int ret = 0;
	asndcap_t snd = 0;
	struct sl_xcore_request req;
	volatile thdid_t xcore_tid = 0;

	SL_XCORE_REQ(req, SL_XCORE_THD_ALLOC, (vaddr_t)&xcore_tid);
	req.sl_xcore_req_thd_alloc.fn = fn;
	req.sl_xcore_req_thd_alloc.data = data;
	if (nparams) memcpy(req.sl_xcore_req_thd_alloc.params, params, sizeof(sched_param_t) * nparams);
	req.sl_xcore_req_thd_alloc.param_count = nparams;

	ret = _sl_xcore_request_enqueue(core, &req);
	if (unlikely(ret)) return NULL;

	/* Other core will wake this up after creation! */
	if (sl_thd_curr() != sl__globals_core()->sched_thd) {
		sl_thd_block(0);
	} else {
		while (!xcore_tid) sl_thd_yield(0);
	}
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
	struct sl_xcore_thd *t = sl_xcore_thd_lookup(tid, core);

	sl_xcore_thd_wakeup(t);
}

/******************************* Server-side ***************************/

static inline int
_sl_xcore_req_thd_alloc_no_cs(struct sl_xcore_request *req)
{
	cos_thd_fn_t   fn   = req->sl_xcore_req_thd_alloc.fn;
	void          *data = req->sl_xcore_req_thd_alloc.data;
	struct sl_thd *t;
	int i;

	assert(fn);

	t = sl_thd_alloc_no_cs(fn, data);
	assert(t);
	if (likely(req->response)) *((thdid_t *)req->response) = sl_thd_thdid(t);
	for (i = 0; i < req->sl_xcore_req_thd_alloc.param_count; i++) sl_thd_param_set(t, req->sl_xcore_req_thd_alloc.params[i]);
	_sl_xcore_thd_wakeup_tid_no_cs(req->client_thd, req->client_core);

	return 0;
}

static inline int
_sl_xcore_req_thd_param_set_no_cs(struct sl_xcore_request *req)
{
	struct sl_thd *t = sl_thd_lkup(req->sl_xcore_req_thd_param_set.tid);

	if (!t) return -1;
	sl_thd_param_set(t, req->sl_xcore_req_thd_param_set.param);

	return 0;
}

static inline int
_sl_xcore_req_thd_wakeup_no_cs(struct sl_xcore_request *req)
{
	struct sl_thd *t = sl_thd_lkup(req->sl_xcore_req_thd_param_set.tid);

	if (!t) return -1;
	sl_thd_wakeup_no_cs(t);

	return 0;
}

int
sl_xcore_process_no_cs(void)
{
	int num = 0;
	struct sl_xcore_request xcore_req;

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
		default:
		{
			PRINTC("Unrecognized request! Aborting!\n");
			assert(0);
		}
		}
		num ++;
	}

	return num; /* number of requests processed */
}
