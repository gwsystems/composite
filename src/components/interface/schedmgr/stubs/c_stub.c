#include <schedmgr.h>
#include <cos_thd_init.h>

int schedmgr_thd_block_timeout_intern(spdid_t c, thdid_t deptid, u32_t abs_hi, u32_t abs_lo);
thdid_t schedmgr_thd_create_intern(spdid_t c, int idx);
thdid_t schedmgr_aep_create_intern(spdid_t c, int idx, int owntc, arcvcap_t *rcv, u32_t *unused);

int
schedmgr_thd_block_timeout(spdid_t c, thdid_t deptid, cycles_t abs_timeout)
{
	return schedmgr_thd_block_timeout_intern(c, deptid, (u32_t)(abs_timeout >> 32), (u32_t)((abs_timeout << 32) >> 32));
}

thdid_t
schedmgr_thd_create(spdid_t c, cos_thd_fn_t fn, void *data)
{
	int idx = cos_thd_init_alloc(fn, data);

	if (idx < 1) assert(0);

	return schedmgr_thd_create_intern(c, idx);
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
schedmgr_aep_create(spdid_t c, struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data, int owntc)
{
	int idx = cos_thd_init_alloc(__schedmgr_aep_fn, (void *)aep);
	arcvcap_t rcv;
	int ret;
	u32_t unused;

	if (idx < 1) assert(0);

	ret = schedmgr_aep_create_intern(c, idx, owntc, &rcv, &unused);
	assert(ret);

	aep->fn = fn;
	aep->data = data;
	aep->thd = 0;
	aep->rcv = rcv;
	aep->tc = 0;

	return ret;
}
