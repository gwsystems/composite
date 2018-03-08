#include <cos_defkernel_api.h>
#include <sched.h>
#include <sl.h>
#include "sched_info.h"

int
sched_thd_wakeup_intern(spdid_t c, thdid_t t)
{
	if (!c || !sched_childinfo_find(c)) return -1;
	sl_thd_wakeup(t);

	return 0;
}

int
sched_thd_block_intern(spdid_t c, thdid_t deptid)
{
	if (!c || !sched_childinfo_find(c)) return -1;
	sl_thd_block(deptid);

	return 0;
}

int
sched_thd_block_timeout_intern(spdid_t c, thdid_t deptid, u32_t hi, u32_t lo)
{
	if (!c || !sched_childinfo_find(c)) return -1;
	/* TODO: return time elapsed */
	sl_thd_block_timeout(deptid, ((cycles_t)hi << 32 | (cycles_t)lo));

	return 0;
}

thdid_t
sched_thd_create_intern(spdid_t c, int idx)
{
	struct cos_defcompinfo *dci;
	struct sl_thd *t = NULL;

	if (!c) return 0;
	dci = sched_child_defci_get(sched_childinfo_find(c));
	if (!dci) return 0;

	t = sl_thd_ext_idx_alloc(dci, idx);
	if (!t) return 0;

	return sl_thd_thdid(t);
}

thdid_t
sched_aep_create_intern(spdid_t c, int idx, int owntc, int u1, arcvcap_t *extrcv, int *u2)
{
	struct cos_defcompinfo *dci;
	struct sl_thd *t = NULL;

	if (!c) return 0;
	dci = sched_child_defci_get(sched_childinfo_find(c));
	if (!dci) return 0;

	t = sl_thd_extaep_idx_alloc(dci, sl__globals()->sched_thd, idx, owntc, extrcv);
	if (!t) return 0;

	return sl_thd_thdid(t);
}

int
sched_thd_param_set_intern(spdid_t c, thdid_t tid, sched_param_t sp)
{
	struct sl_thd *t = sl_thd_lkup(tid);

	if (!c || !sched_childinfo_find(c)) return -1;
	if (!t) return -1;
	sl_thd_param_set(t, sp);

	return 0;
}

int
sched_thd_delete_intern(spdid_t c, thdid_t tid)
{
	struct sl_thd *t = sl_thd_lkup(tid);

	if (!c || !sched_childinfo_find(c)) return -1;
	if (!t) return -1;

	sl_thd_free(t);

	return 0;
}

int
sched_thd_exit_intern(spdid_t c)
{
	if (!c || !sched_childinfo_find(c)) return -1;
	sl_thd_exit();

	return 0;
}
