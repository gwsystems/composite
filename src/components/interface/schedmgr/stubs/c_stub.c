#include <schedmgr.h>
#include <cos_thd_init.h>

int schedmgr_thd_wakeup_intern(spdid_t c, thdid_t t);
int schedmgr_thd_block_intern(spdid_t c, thdid_t dep_t);
int schedmgr_thd_block_timeout_intern(spdid_t c, thdid_t deptid, u32_t abs_hi, u32_t abs_lo);
thdid_t schedmgr_thd_create_intern(spdid_t c, int idx);
thdid_t schedmgr_aep_create_intern(spdid_t c, int idx, int owntc, int u1, arcvcap_t *rcv, u32_t *unused);

int schedmgr_thd_param_set_intern(spdid_t c, thdid_t tid, sched_param_t p);
int schedmgr_thd_delete_intern(spdid_t c, thdid_t tid);
int schedmgr_thd_exit_intern(spdid_t c);

int
schedmgr_thd_wakeup(thdid_t t)
{
	return schedmgr_thd_wakeup_intern(0, t);
}

int
schedmgr_thd_block(thdid_t dt)
{
	return schedmgr_thd_block_intern(0, dt);
}

int
schedmgr_thd_block_timeout(thdid_t deptid, cycles_t abs_timeout)
{
	return schedmgr_thd_block_timeout_intern(0, deptid, (u32_t)(abs_timeout >> 32),
						 (u32_t)((abs_timeout << 32) >> 32));
}

thdid_t
schedmgr_thd_create(cos_thd_fn_t fn, void *data)
{
	int idx = cos_thd_init_alloc(fn, data);

	if (idx < 1) return 0;

	return schedmgr_thd_create_intern(0, idx);
}

static void
__schedmgr_aep_fn(void *data)
{
	struct cos_aep_info *ai = (struct cos_aep_info *)data;
	cos_aepthd_fn_t      fn = ai->fn;
	void *               fndata = ai->data;

	(fn)(ai->rcv, fndata);
}

thdid_t
schedmgr_aep_create(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data, int owntc)
{
	int idx = cos_thd_init_alloc(__schedmgr_aep_fn, (void *)aep);
	arcvcap_t rcv;
	int ret;
	u32_t unused;

	if (idx < 1) return 0;

	ret = schedmgr_aep_create_intern(0, idx, owntc, unused, &rcv, &unused);
	if (!ret) return 0;

	aep->fn = fn;
	aep->data = data;
	aep->thd = 0;
	aep->rcv = rcv;
	aep->tc = 0;

	return ret;
}

int
schedmgr_thd_param_set(thdid_t t, sched_param_t p)
{
	return schedmgr_thd_param_set_intern(0, t, p);
}

int
schedmgr_thd_delete(thdid_t t)
{
	return schedmgr_thd_delete_intern(0, t);
}

int
schedmgr_thd_exit(void)
{
	return schedmgr_thd_exit_intern(0);
}
