/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cos_defkernel_api.h>
#include <sched.h>
#include <sl.h>
#include <sched_info.h>

int
sched_thd_wakeup(thdid_t t)
{
	spdid_t c = cos_inv_token();

	if (!c || !sched_childinfo_find(c)) return -1;
	sl_thd_wakeup(t);

	return 0;
}

int
sched_thd_block(thdid_t deptid)
{
	spdid_t c = cos_inv_token();

	if (!c || !sched_childinfo_find(c)) return -1;
	sl_thd_block(deptid);

	return 0;
}

int
sched_thd_block_timeout_cserialized(u32_t *elapsed_hi, u32_t *elapsed_lo, thdid_t deptid, u32_t hi, u32_t lo)
{
	spdid_t c = cos_inv_token();
	cycles_t elapsed = 0;

	if (!c || !sched_childinfo_find(c)) return -1;
	elapsed = sl_thd_block_timeout(deptid, ((cycles_t)hi << 32 | (cycles_t)lo));
	*elapsed_hi = (elapsed >> 32);
	*elapsed_lo = (elapsed << 32) >> 32;

	return 0;
}

thdid_t
sched_thd_create_cserialized(thdclosure_index_t idx)
{
	spdid_t c = cos_inv_token();
	struct cos_defcompinfo *dci;
	struct sl_thd *t = NULL;

	if (!c) return 0;
	dci = sched_child_defci_get(sched_childinfo_find(c));
	if (!dci) return 0;

	t = sl_thd_aep_alloc_ext(dci, NULL, idx, 0, 0, 0, 0, 0, NULL);
	if (!t) return 0;
	srv_dummy_thdinit(sl_thd_thdid(t), 0);

	return sl_thd_thdid(t);
}

thdid_t
sched_aep_create_cserialized(arcvcap_t *extrcv, int *unused, u32_t thdidx_owntc, u32_t key_ipimax, u32_t ipiwin32b)
{
	spdid_t c = cos_inv_token();
	struct cos_defcompinfo *dci;
	struct sl_thd      *t      = NULL;
	int                 owntc  = (thdidx_owntc << 16) >> 16;
	thdclosure_index_t  idx    = (thdidx_owntc >> 16);
	microsec_t          ipiwin = (microsec_t)ipiwin32b;
	u32_t               ipimax = (key_ipimax << 16) >> 16;
	cos_channelkey_t    key    = (key_ipimax >> 16);

	if (!c) return 0;
	dci = sched_child_defci_get(sched_childinfo_find(c));
	if (!dci) return 0;

	t = sl_thd_aep_alloc_ext(dci, NULL, idx, 1, owntc, key, ipiwin, ipimax, extrcv);
	if (!t) return 0;
	srv_dummy_thdinit(sl_thd_thdid(t), 1);

	return sl_thd_thdid(t);
}

int
sched_thd_param_set(thdid_t tid, sched_param_t sp)
{
	spdid_t c = cos_inv_token();
	struct sl_thd *t = sl_thd_lkup(tid);

	if (!c || !sched_childinfo_find(c)) return -1;
	if (!t) return -1;
	sl_thd_param_set(t, sp);

	return 0;
}

int
sched_thd_delete(thdid_t tid)
{
	spdid_t c = cos_inv_token();
	struct sl_thd *t = sl_thd_lkup(tid);

	if (!c || !sched_childinfo_find(c)) return -1;
	if (!t) return -1;

	sl_thd_free(t);

	return 0;
}

int
sched_thd_exit(void)
{
	spdid_t c = cos_inv_token();

	if (!c || !sched_childinfo_find(c)) return -1;
	sl_thd_exit();

	return 0;
}
