#include <ps.h>
#include <ck_ring.h>
#include <sl_xcore.h>
#include <sl.h>
#include <bitmap.h>

#define SL_REQ_THD_ALLOC(req, fn, data) do {							\
						req.type = SL_XCORE_THD_ALLOC;			\
						req.client = cos_cpuid();			\
						req.req_response = 0;				\
						req.sl_xcore_req_thd_alloc.fn = fn;		\
						req.sl_xcore_req_thd_alloc.data = data;		\
					     } while (0)

extern struct sl_thd *sl_thd_alloc_no_cs(cos_thd_fn_t fn, void *data);

int
sl_xcore_thd_alloc(cpuid_t core, cos_thd_fn_t fn, void *data, int nparams, sched_param_t params[])
{
	int ret = 0;
	asndcap_t snd = 0;
	struct sl_xcore_request req;

	if (core == cos_cpuid()) return -EINVAL;
	if (!bitmap_check(sl__globals()->core_bmp, core)) return -EINVAL;

	sl_cs_enter();

	SL_REQ_THD_ALLOC(req, fn, data);
	if (nparams) memcpy(req.params, params, sizeof(sched_param_t) * nparams);
	req.param_count = nparams;
	if (ck_ring_enqueue_mpsc_xcore(sl__ring(core), sl__ring_buffer(core), &req) != true) {
		ret = -ENOMEM;
	} else {
		snd = sl__globals()->xcore_asnd[cos_cpuid()][core];
		assert(snd);
	}

	sl_cs_exit();
	/* if (!snd) return -1; */
	/* send an IPI for the request */
	/* cos_asnd(snd, 0); */

	return ret;
}

int
sl_xcore_thd_alloc_ext(cpuid_t core, struct cos_defcompinfo *dci, thdclosure_index_t idx, int nparams, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcore_aep_alloc(cpuid_t core, cos_thd_fn_t fn, void *data, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcore_aep_alloc_ext(cpuid_t core, struct cos_defcompinfo *dci, thdclosure_index_t idx, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcore_initaep_alloc(cpuid_t core, struct cos_defcompinfo *dci, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcore_initaep_alloc_ext(cpuid_t core, struct cos_defcompinfo *dci, struct cos_defcompinfo *sched, int own_tcap, cos_channelkey_t key, int nparams, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcore_process_no_cs(void)
{
	int num = 0;
	struct sl_xcore_request xcore_req;

	while (ck_ring_dequeue_mpsc_xcore(sl__ring_curr(), sl__ring_buffer_curr(), &xcore_req) == true) {

		assert(xcore_req.client != cos_cpuid());
		switch(xcore_req.type) {
		case SL_XCORE_THD_ALLOC:
		{
			cos_thd_fn_t   fn   = xcore_req.sl_xcore_req_thd_alloc.fn;
			void          *data = xcore_req.sl_xcore_req_thd_alloc.data;
			struct sl_thd *t;
			int i;

			assert(fn);

			t = sl_thd_alloc_no_cs(fn, data);
			assert(t);
			for (i = 0; i < xcore_req.param_count; i++) {
				sl_thd_param_set(t, xcore_req.params[i]);
			}

			break;
		}
		case SL_XCORE_THD_ALLOC_EXT:
		case SL_XCORE_AEP_ALLOC:
		case SL_XCORE_AEP_ALLOC_EXT:
		case SL_XCORE_INITAEP_ALLOC:
		case SL_XCORE_THD_DEALLOC:
		default:
		{
			PRINTC("Unimplemented request! Aborting!\n");
			assert(0);
		}
		}
		num ++;
	}

	return num; /* number of requests processed */
}
