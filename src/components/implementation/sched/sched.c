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
	struct sched_childinfo *sci;

	if (!c) return 0;
	sci = sched_childinfo_find(c);
	if (!sci) return 0;

	return sched_child_thd_create(sci, idx);
}

thdid_t
sched_aep_create_cserialized(arcvcap_t *extrcv, int *unused, thdclosure_index_t idx, int owntc, cos_channelkey_t key)
{
	spdid_t c = cos_inv_token();
	struct sched_childinfo *sci;

	if (!c) return 0;
	sci = sched_childinfo_find(c);
	if (!sci) return 0;

	return sched_child_aep_create(sci, idx, owntc, key, extrcv);
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
