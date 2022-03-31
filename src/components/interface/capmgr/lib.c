#include <capmgr.h>
#include <cos_thd_init.h>

thdcap_t
capmgr_thd_create(cos_thd_fn_t fn, void *data, thdid_t *tid, struct cos_dcb_info *dcb)
{
	thdclosure_index_t idx = cos_thd_init_alloc(fn, data);

	if (idx < 1) return 0;

	return capmgr_thd_create_thunk(idx, tid, dcb);
}

thdcap_t
capmgr_aep_create(struct cos_aep_info *a, cos_aepthd_fn_t fn, void *data, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, struct cos_dcb_info *dcb)
{
	thdclosure_index_t idx = cos_thd_init_alloc(cos_aepthd_fn, (void *)a);

	if (idx < 1) return 0;
	a->fn   = fn;
	a->data = data;

	return capmgr_aep_create_thunk(a, idx, owntc, key, ipiwin, ipimax, dcb);
}
