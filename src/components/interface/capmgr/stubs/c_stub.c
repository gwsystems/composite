#include <memmgr.h>
#include <capmgr.h>
#include <cos_thd_init.h>
#include <cos_defkernel_api.h>

thdcap_t capmgr_initthd_create_cserialized(thdid_t *tid, int *unused, spdid_t s);
thdcap_t capmgr_initaep_create_cserialized(u32_t *sndtidret, u32_t *rcvtcret, spdid_t s, int owntc, cos_channelkey_t key);
thdcap_t capmgr_thd_create_cserialized(struct cos_dcb_info **dcb, thdid_t *tid, thdclosure_index_t idx);
u32_t    capmgr_aep_create_cserialized(struct cos_dcb_info **dcb, u32_t *tcrcvret, thdclosure_index_t idx, int owntc, cos_channelkey_t key);
thdcap_t capmgr_thd_create_ext_cserialized(struct cos_dcb_info **dcb, thdid_t *tid, spdid_t s, thdclosure_index_t idx);
/* rcvcap for spdid = s shall be obtained through a separate call to capmgr! */
arcvcap_t capmgr_aep_rcv_retrieve_cserialized(spdid_t s, thdid_t tid);
u32_t    capmgr_aep_create_ext_cserialized(struct cos_dcb_info **dcb, u32_t *rcvtcret, spdid_t s, thdclosure_index_t idx, u32_t owntc_aepkey);
thdcap_t capmgr_thd_retrieve_next_cserialized(thdid_t *tid, int *unused, spdid_t s);
thdcap_t capmgr_thd_retrieve_cserialized(thdid_t *inittid, int *unused, spdid_t s, thdid_t tid);

thdcap_t
capmgr_thd_retrieve(spdid_t child, thdid_t tid, thdid_t *inittid)
{
	int unused;

	return capmgr_thd_retrieve_cserialized(inittid, &unused, child, tid);
}

thdcap_t
capmgr_thd_retrieve_next(spdid_t child, thdid_t *tid)
{
	int unused;

	return capmgr_thd_retrieve_next_cserialized(tid, &unused, child);
}

thdcap_t
capmgr_initthd_create(spdid_t child, thdid_t *tid)
{
	int unused;

	return capmgr_initthd_create_cserialized(tid, &unused, child);
}

thdcap_t
capmgr_thd_create(cos_thd_fn_t fn, void *data, thdid_t *tid, struct cos_dcb_info **dcb)
{
	thdclosure_index_t idx = cos_thd_init_alloc(fn, data);

	if (idx < 1) return 0;

	return capmgr_thd_create_cserialized(dcb, tid, idx);
}

thdcap_t
capmgr_thd_create_ext(spdid_t child, thdclosure_index_t idx, thdid_t *tid, struct cos_dcb_info **dcb)
{
	return capmgr_thd_create_ext_cserialized(dcb, tid, child, idx);
}

thdcap_t
capmgr_aep_create(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data, int owntc, cos_channelkey_t key, struct cos_dcb_info **dcb)
{
	u32_t tcrcvret = 0, thdtidret = 0;
	thdcap_t thd = 0;
	arcvcap_t rcv = 0;
	tcap_t tc = 0;
	thdid_t tid = 0;
	thdclosure_index_t idx = cos_thd_init_alloc(cos_aepthd_fn, (void *)aep);

	if (idx < 1) return 0;

	thdtidret = capmgr_aep_create_cserialized(dcb, &tcrcvret, idx, owntc, key);
	if (!thdtidret) return 0;
	thd = thdtidret >> 16;
	tid = (thdtidret << 16) >> 16;
	if (!thd || !tid) return 0;

	aep->fn   = fn;
	aep->data = data;
	aep->thd  = thd;
	aep->rcv  = (tcrcvret << 16) >> 16;
	aep->tc   = (tcrcvret >> 16);
	aep->tid  = tid;

	return aep->thd;
}

thdcap_t
capmgr_aep_create_ext(spdid_t child, struct cos_aep_info *aep, thdclosure_index_t idx, int owntc, cos_channelkey_t key, struct cos_dcb_info **dcb, arcvcap_t *extrcv)
{
	u32_t thdtidret = 0;
	u32_t tcrcvret = 0;
	thdid_t tid = 0;
	thdcap_t thd = 0;
	u32_t owntc_aepkey = (owntc << 16) | (key);

	thdtidret = capmgr_aep_create_ext_cserialized(dcb, &tcrcvret, child, idx, owntc_aepkey);
	if (!thdtidret) return thd;
	thd = thdtidret >> 16;
	tid = (thdtidret << 16) >> 16;
	if (!thd || !tid) return 0;

	aep->fn   = NULL;
	aep->data = NULL;
	aep->thd  = thd;
	aep->tid  = tid;
	aep->rcv  = tcrcvret >> 16;
	aep->tc   = (tcrcvret << 16) >> 16;
	*extrcv = capmgr_aep_rcv_retrieve_cserialized(child, tid);
	assert(*extrcv);

	return aep->thd;
}

thdcap_t
capmgr_initaep_create(spdid_t child, struct cos_aep_info *aep, int owntc, cos_channelkey_t key, asndcap_t *snd)
{
	thdcap_t thd = 0;
	u32_t sndtidret = 0, rcvtcret = 0;

	thd = capmgr_initaep_create_cserialized(&sndtidret, &rcvtcret, child, owntc, key);
	if (!thd) return thd;

	aep->fn   = NULL;
	aep->data = NULL;
	aep->thd  = thd;
	aep->tid  = (sndtidret << 16) >> 16;
	aep->rcv  = rcvtcret >> 16;
	aep->tc   = (rcvtcret << 16) >> 16;
	*snd      = sndtidret >> 16;

	/* initcaps are copied to INITXXX offsets in the dst component */
	return aep->thd;
}

cbuf_t memmgr_shared_page_allocn_cserialized(vaddr_t *pgaddr, int *unused, unsigned long num_pages);
unsigned long memmgr_shared_page_map_cserialized(vaddr_t *pgaddr, int *unused, cbuf_t id);

vaddr_t
memmgr_heap_page_alloc(void)
{
	return memmgr_heap_page_allocn(1);
}

cbuf_t
memmgr_shared_page_allocn(unsigned long num_pages, vaddr_t *pgaddr)
{
	int unused = 0;

	return memmgr_shared_page_allocn_cserialized(pgaddr, &unused, num_pages);
}

cbuf_t
memmgr_shared_page_alloc(vaddr_t *pgaddr)
{
	return memmgr_shared_page_allocn(1, pgaddr);
}

unsigned long
memmgr_shared_page_map(cbuf_t id, vaddr_t *pgaddr)
{
	int unused = 0;

	return memmgr_shared_page_map_cserialized(pgaddr, &unused, id);
}
