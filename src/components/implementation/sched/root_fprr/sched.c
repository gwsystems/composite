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
#include <sl_blkpt.h>

int
sched_thd_yield_to(thdid_t t)
{
	spdid_t c = cos_inv_token();

	if (!c || !sched_childinfo_find(c)) return -1;
	sl_thd_yield(t);

	return 0;
}

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

cycles_t
sched_thd_block_timeout(thdid_t dep_id, cycles_t abs_timeout)
{
	spdid_t c = cos_inv_token();

	if (!c || !sched_childinfo_find(c)) return 0;
	return sl_thd_block_timeout(dep_id, abs_timeout);
}

sched_blkpt_id_t
sched_blkpt_alloc(void)
{
	return sl_blkpt_alloc();
}

int
sched_blkpt_free(sched_blkpt_id_t id)
{
	return sl_blkpt_free(id);
}

int
sched_blkpt_trigger(sched_blkpt_id_t blkpt, sched_blkpt_epoch_t epoch, int single)
{
	return sl_blkpt_trigger(blkpt, epoch, single);
}

int
sched_blkpt_block(sched_blkpt_id_t blkpt, sched_blkpt_epoch_t epoch, thdid_t dependency)
{
	return sl_blkpt_block(blkpt, epoch, dependency);
}

thdid_t
sched_thd_create_closure(thdclosure_index_t idx)
{
	spdid_t c = cos_inv_token();
	struct cos_defcompinfo *dci;
	struct sl_thd *t = NULL;

	if (!c) return 0;
	dci = sched_child_defci_get(sched_childinfo_find(c));
	if (!dci) return 0;

	t = sl_thd_aep_alloc_ext(dci, NULL, idx, 0, 0, 0, 0, 0, NULL);
	if (!t) return 0;

	return sl_thd_thdid(t);
}

thdid_t
sched_aep_create_closure(thdclosure_index_t id, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *rcv)
{
	spdid_t c = cos_inv_token();
	struct cos_defcompinfo *dci;
	struct sl_thd *t = NULL;

	if (!c) return 0;
	dci = sched_child_defci_get(sched_childinfo_find(c));
	if (!dci) return 0;

	t = sl_thd_aep_alloc_ext(dci, NULL, id, 1, owntc, key, ipiwin, ipimax, rcv);
	if (!t) return 0;

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
