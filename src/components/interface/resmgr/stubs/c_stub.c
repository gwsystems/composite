#include <resmgr.h>
#include <cos_thd_init.h>
#include <cos_defkernel_api.h>

thdcap_t resmgr_thd_create_intern(spdid_t c, int idx);
thdcap_t resmgr_aep_create_intern(spdid_t c, int idx, int owntc, arcvcap_t *rcvret, tcap_t *tcret);
thdcap_t resmgr_initaep_create_intern(spdid_t c, spdid_t s, int owntc, asndcap_t *sndret, u32_t *unused);

thdcap_t
resmgr_thd_create(spdid_t c, cos_thd_fn_t fn, void *data)
{
	int idx = cos_thd_init_alloc(fn, data);

	if (idx < 1) assert(0);

	return resmgr_thd_create_intern(0, idx);
}

static void
__resmgr_aep_fn(void *data)
{
	struct cos_aep_info *ai    = (struct cos_aep_info *)data;
	cos_aepthd_fn_t      fn    = ai->fn;
	void *               fdata = ai->data;

	(fn)(ai->rcv, fdata);
}

thdcap_t
resmgr_aep_create(spdid_t c, struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data, int owntc)
{
	int idx = cos_thd_init_alloc(__resmgr_aep_fn, (void *)aep);
	int ret;
	arcvcap_t rcv;
	tcap_t tc;

	if (idx < 1) assert(0);

	ret = resmgr_aep_create_intern(0, idx, owntc, &rcv, &tc);
	assert(ret > 0);

	aep->fn   = fn;
	aep->data = data;
	aep->thd  = ret;
	aep->rcv  = rcv;
	aep->tc   = tc;

	return 0;
}

thdcap_t
resmgr_initaep_create(spdid_t c, spdid_t s, int owntc, asndcap_t *snd)
{
	int ret;
	u32_t unused;

	ret = resmgr_initaep_create_intern(0, s, owntc, snd, &unused);
	assert(ret > 0);

	/* rcv, tc => are copied to INITRCV, INITTCAP offsets */
	return ret;
}
