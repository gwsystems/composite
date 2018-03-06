#include <cos_defkernel_api.h>
#include <schedmgr.h>
#include <sl.h>

extern u64_t child_spdbits;
extern struct cos_defcompinfo *child_defci_get(spdid_t spdid);

#define IS_CHILD(c) (child_spdbits & ((u64_t)1 << (c-1)))

int
schedmgr_thd_wakeup_intern(spdid_t c, thdid_t t)
{
	if (!c || !IS_CHILD(c)) return -1;
	sl_thd_wakeup(t);

	return 0;
}

int
schedmgr_thd_block_intern(spdid_t c, thdid_t deptid)
{
	if (!c || !IS_CHILD(c)) return -1;
	sl_thd_block(deptid);

	return 0;
}

int
schedmgr_thd_block_timeout_intern(spdid_t c, thdid_t deptid, u32_t hi, u32_t lo)
{
	if (!c || !IS_CHILD(c)) return -1;
	/* TODO: return time elapsed */
	sl_thd_block_timeout(deptid, ((cycles_t)hi << 32 | (cycles_t)lo));

	return 0;
}

thdid_t
schedmgr_thd_create_intern(spdid_t c, int idx)
{
	struct cos_defcompinfo *dci;
	struct sl_thd *t = NULL;

	if (!c || !IS_CHILD(c)) return 0;
	dci = child_defci_get(c);
	if (!dci) return 0;

	t = sl_thd_ext_idx_alloc(dci, idx);
	if (!t) return 0;

	return t->thdid;
}

thdid_t
schedmgr_aep_create_intern(spdid_t c, int idx, int owntc, int u1, arcvcap_t *extrcv, int *u2)
{
	struct cos_defcompinfo *dci;
	struct sl_thd *t = NULL;

	if (!c || !IS_CHILD(c)) return 0;
	dci = child_defci_get(c);
	if (!dci) return 0;

	t = sl_thd_extaep_idx_alloc(dci, sl__globals()->sched_thd, idx, owntc, extrcv);
	if (!t) return 0;

	return t->thdid;
}

int
schedmgr_thd_param_set_intern(spdid_t c, thdid_t tid, sched_param_t sp)
{
	struct sl_thd *t = sl_thd_lkup(tid);

	if (!c || !IS_CHILD(c)) return -1;
	if (!t) return -1;
	sl_thd_param_set(t, sp);

	return 0;
}

int
schedmgr_thd_delete_intern(spdid_t c, thdid_t tid)
{
	struct sl_thd *t = sl_thd_lkup(tid);

	if (!c || !IS_CHILD(c)) return -1;
	if (!t) return -1;

	sl_thd_free(t);

	return 0;
}

int
schedmgr_thd_exit_intern(spdid_t c)
{
	if (!c || !IS_CHILD(c)) return -1;
	sl_thd_exit();

	return 0;
}
