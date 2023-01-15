/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu & Gabe Parmer, gparmer@gwu.edu
 */

#include <sched.h>
#include <cos_thd_init.h>
#include <cos_stubs.h>

COS_CLIENT_STUB(cycles_t, sched_thd_block_timeout, thdid_t dep_id, cycles_t abs_timeout)
{
	COS_CLIENT_INVCAP;
	word_t elapsed_hi = 0, elapsed_lo = 0;
	cycles_t elapsed_cycles = 0;
	word_t abs_hi, abs_lo;

	COS_ARG_DWORD_TO_WORD(abs_timeout, abs_hi, abs_lo);
	cos_sinv_2rets(uc, dep_id, abs_hi, abs_lo, 0, &elapsed_hi, &elapsed_lo);
	elapsed_cycles = ((cycles_t)elapsed_hi << 32) | (cycles_t)elapsed_lo;

	return elapsed_cycles;
}

COS_CLIENT_STUB(thdid_t, sched_aep_create_closure, thdclosure_index_t id, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *rcv)
{
	COS_CLIENT_INVCAP;
	u32_t idx_owntc = (id << 16) | owntc;
	u32_t key_ipimax = (key << 16) | ipimax;
	u32_t ipiwin32b = ipiwin;
	int ret;
	word_t unused, r;

	if (id < 1) return 0;

	ret = cos_sinv_2rets(uc, idx_owntc, key_ipimax, 0, ipiwin32b, &r, &unused);
	*rcv = r;

	return ret;
}

#ifdef NIL
thdid_t sched_thd_create_cserialized(thdclosure_index_t idx);
thdid_t sched_aep_create_cserialized(arcvcap_t *rcv, int *unused, u32_t thdidx_owntc, u32_t key_ipimax, u32_t ipiwin);

cycles_t
sched_thd_block_timeout(thdid_t deptid, cycles_t abs_timeout)
{
	u32_t elapsed_hi = 0, elapsed_lo = 0;
	cycles_t elapsed_cycles = 0;
	int ret = 0;
	word_t abs_hi, abs_lo;

	COS_ARG_DWORD_TO_WORD(abs_timeout, abs_hi, abs_lo);
	ret = sched_thd_block_timeout_cserialized(&elapsed_hi, &elapsed_lo, deptid, abs_hi, abs_hi);
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
sched_aep_create(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax)
{
	thdclosure_index_t idx = cos_thd_init_alloc(cos_aepthd_fn, (void *)aep);
	arcvcap_t rcv;
	u32_t idx_owntc = (idx << 16) | owntc;
	u32_t key_ipimax = (key << 16) | ipimax;
	u32_t ipiwin32b = ipiwin;
	int ret;
	int unused;

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
