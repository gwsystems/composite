#include <cos_defkernel_api.h>
#include <schedmgr.h>
#include <sl.h>

int
schedmgr_thd_wakeup(spdid_t c, thdid_t t)
{
	sl_thd_wakeup(t);

	return 0;
}

int
schedmgr_thd_block(spdid_t c, thdid_t deptid)
{
	sl_thd_block(deptid);

	return 0;
}

int
schedmgr_thd_block_timeout_intern(spdid_t c, thdid_t deptid, u32_t hi, u32_t lo)
{
	/* TODO: return time elapsed */
	sl_thd_block_timeout(deptid, ((cycles_t)hi << 32 | (cycles_t)lo));

	return 0;
}

thdid_t
schedmgr_thd_create_intern(spdid_t c, int idx)
{
	/* FIXME: use initialized defci */
	struct cos_defcompinfo defcinfo;
	struct sl_thd *t = NULL;

	/* assuming this scheduler uses sl_resmgr library */
	memset(&defcinfo, 0, sizeof(struct cos_defcompinfo));
	defcinfo.id = c;

	t = sl_thd_ext_idx_alloc(&defcinfo, idx);
	if (!t) return 0;

	return t->thdid;
}

thdid_t
schedmgr_aep_create_intern(spdid_t c, int idx, int owntc, arcvcap_t *extrcv, u32_t *unused)
{
	/* FIXME: use initialized defci */
	struct cos_defcompinfo defcinfo;
	struct sl_thd *t = NULL;

	/* assuming this scheduler uses sl_resmgr library */
	memset(&defcinfo, 0, sizeof(struct cos_defcompinfo));
	defcinfo.id = c;

	t = sl_thd_extaep_idx_alloc(&defcinfo, sl__globals()->sched_thd, idx, owntc, extrcv);
	if (!t) return 0;

	return t->thdid;
}

int
schedmgr_thd_param_set(spdid_t c, thdid_t tid, sched_param_t sp)
{
	struct sl_thd *t = sl_thd_lkup(tid);

	assert(t);
	sl_thd_param_set(t, sp);

	return 0;
}

int
schedmgr_thd_delete(spdid_t c, thdid_t tid)
{
	struct sl_thd *t = sl_thd_lkup(tid);

	assert(t);

	sl_thd_free(t);

	return 0;
}

int
schedmgr_thd_exit(spdid_t c)
{
	sl_thd_exit();

	return 0;
}
