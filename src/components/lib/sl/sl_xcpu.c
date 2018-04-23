/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <ps.h>
#include <ck_ring.h>
#include <sl_xcpu.h>
#include <sl.h>
#include <bitmap.h>

#define SL_REQ_THD_ALLOC(req, fn, data) do {							\
						req.type = SL_XCPU_THD_ALLOC;			\
						req.client = cos_cpuid();			\
						req.req_response = 0;				\
						req.sl_xcpu_req_thd_alloc.fn = fn;		\
						req.sl_xcpu_req_thd_alloc.data = data;		\
					     } while (0)

extern struct sl_thd *sl_thd_alloc_no_cs(cos_thd_fn_t fn, void *data);

int
sl_xcpu_thd_alloc(cpuid_t cpu, cos_thd_fn_t fn, void *data, sched_param_t params[])
{
	int i, sz = sizeof(params) / sizeof(params[0]);
	int ret = 0;
	asndcap_t snd = 0;
	struct sl_xcpu_request req;

	if (cpu == cos_cpuid()) return -EINVAL;
	if (!bitmap_check(sl__globals()->cpu_bmp, cpu)) return -EINVAL;

	sl_cs_enter();

	SL_REQ_THD_ALLOC(req, fn, data);
	memcpy(req.params, params, sizeof(sched_param_t) * sz);
	req.param_count = sz;
	if (ck_ring_enqueue_mpsc_xcpu(sl__ring(cpu), sl__ring_buffer(cpu), &req) != true) {
		ret = -ENOMEM;
	} else {
		snd = sl__globals()->xcpu_asnd[cos_cpuid()][cpu];
		assert(snd);
	}

	sl_cs_exit();

	if (!snd || ret) goto done;

	/* send an IPI for the request */
	ret = cos_asnd(snd, 1);

done:
	return ret;
}

int
sl_xcpu_thd_alloc_ext(cpuid_t cpu, struct cos_defcompinfo *dci, thdclosure_index_t idx, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcpu_aep_alloc(cpuid_t cpu, cos_thd_fn_t fn, void *data, int own_tcap, cos_channelkey_t key, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcpu_aep_alloc_ext(cpuid_t cpu, struct cos_defcompinfo *dci, thdclosure_index_t idx, int own_tcap, cos_channelkey_t key, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcpu_initaep_alloc(cpuid_t cpu, struct cos_defcompinfo *dci, int own_tcap, cos_channelkey_t key, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcpu_initaep_alloc_ext(cpuid_t cpu, struct cos_defcompinfo *dci, struct cos_defcompinfo *sched, int own_tcap, cos_channelkey_t key, sched_param_t params[])
{
	return -ENOTSUP;
}

int
sl_xcpu_process_no_cs(void)
{
	int num = 0;
	struct sl_xcpu_request xcpu_req;

	while (ck_ring_dequeue_mpsc_xcpu(sl__ring_curr(), sl__ring_buffer_curr(), &xcpu_req) == true) {

		assert(xcpu_req.client != cos_cpuid());
		switch(xcpu_req.type) {
		case SL_XCPU_THD_ALLOC:
		{
			cos_thd_fn_t   fn   = xcpu_req.sl_xcpu_req_thd_alloc.fn;
			void          *data = xcpu_req.sl_xcpu_req_thd_alloc.data;
			struct sl_thd *t;
			int i;

			assert(fn);

			t = sl_thd_alloc_no_cs(fn, data);
			assert(t);
			for (i = 0; i < xcpu_req.param_count; i++) {
				sl_thd_param_set(t, xcpu_req.params[i]);
			}

			break;
		}
		case SL_XCPU_THD_ALLOC_EXT:
		case SL_XCPU_AEP_ALLOC:
		case SL_XCPU_AEP_ALLOC_EXT:
		case SL_XCPU_INITAEP_ALLOC:
		case SL_XCPU_THD_DEALLOC:
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
