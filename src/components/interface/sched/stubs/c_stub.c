/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu & Gabe Parmer, gparmer@gwu.edu
 */

#include <sched.h>
#include <cos_thd_init.h>
#include <cos_stubs.h>

COS_CLIENT_STUB(cycles_t, sched_thd_block_timeout)(struct usr_inv_cap *uc, thdid_t dep_id, cycles_t abs_timeout)
{
	word_t   elapsed_hi = 0, elapsed_lo = 0;
	cycles_t elapsed_cycles = 0;

	cos_sinv_2rets(uc->cap_no, dep_id, (u32_t)(abs_timeout >> 32), (u32_t)((abs_timeout << 32) >> 32), 0,
	               &elapsed_hi, &elapsed_lo);
	elapsed_cycles = ((cycles_t)elapsed_hi << 32) | (cycles_t)elapsed_lo;

	return elapsed_cycles;
}

COS_CLIENT_STUB(thdid_t, sched_aep_create_closure)
(struct usr_inv_cap *uc, thdclosure_index_t id, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax,
 arcvcap_t *rcv)
{
	u32_t  idx_owntc  = (id << 16) | owntc;
	u32_t  key_ipimax = (key << 16) | ipimax;
	u32_t  ipiwin32b  = ipiwin;
	int    ret;
	word_t unused, r;

	if (id < 1) return 0;

	ret  = cos_sinv_2rets(uc->cap_no, idx_owntc, key_ipimax, 0, ipiwin32b, &r, &unused);
	*rcv = r;

	return ret;
}

#ifdef NIL
thdid_t sched_thd_create_cserialized(thdclosure_index_t idx);
thdid_t sched_aep_create_cserialized(arcvcap_t *rcv, int *unused, u32_t thdidx_owntc, u32_t key_ipimax, u32_t ipiwin);

cycles_t
sched_thd_block_timeout(thdid_t deptid, cycles_t abs_timeout)
{
	u32_t    elapsed_hi = 0, elapsed_lo = 0;
	cycles_t elapsed_cycles = 0;
	int      ret            = 0;

	ret = sched_thd_block_timeout_cserialized(&elapsed_hi, &elapsed_lo, deptid, (u32_t)(abs_timeout >> 32),
	                                          (u32_t)((abs_timeout << 32) >> 32));
	if (!ret) elapsed_cycles = ((cycles_t)elapsed_hi << 32) | (cycles_t)elapsed_lo;

	return elapsed_cycles;
}

thdid_t
sched_thd_create(cos_thd_fn_t fn, void *data)
{
	thdclosure_index_t idx = cos_thd_init_alloc(fn, data);

	if (idx < 1) return 0;

	return sched_thd_create_cserialized(idx);
}

thdid_t
sched_aep_create(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data, int owntc, cos_channelkey_t key,
                 microsec_t ipiwin, u32_t ipimax)
{
	thdclosure_index_t idx = cos_thd_init_alloc(cos_aepthd_fn, (void *)aep);
	arcvcap_t          rcv;
	u32_t              idx_owntc  = (idx << 16) | owntc;
	u32_t              key_ipimax = (key << 16) | ipimax;
	u32_t              ipiwin32b  = ipiwin;
	int                ret;
	int                unused;

	if (idx < 1) return 0;

	memset(aep, 0, sizeof(struct cos_aep_info));
	ret = sched_aep_create_cserialized(&rcv, &unused, idx_owntc, key_ipimax, ipiwin32b);
	if (!ret) return 0;

	aep->fn   = fn;
	aep->data = data;
	aep->rcv  = rcv;
	aep->tid  = ret;

	return ret;
}
#endif
