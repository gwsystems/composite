#include <memmgr.h>
#include <capmgr.h>
#include <cos_thd_init.h>
#include <cos_defkernel_api.h>

thdcap_t capmgr_initthd_create_cserialized(thdid_t *tid, int *unused, spdid_t s);
thdcap_t capmgr_thd_create_cserialized(thdid_t *tid, int *unused, thdclosure_index_t idx);
thdcap_t capmgr_ext_thd_create_cserialized(thdid_t *tid, int *unused, spdid_t s, thdclosure_index_t idx);
thdcap_t capmgr_aep_create_cserialized(thdid_t *tid, u32_t *tcrcvret, thdclosure_index_t idx, int owntc);
thdcap_t capmgr_ext_aep_create_cserialized(u32_t *drcvtidret, u32_t *rcvtcret, spdid_t s, thdclosure_index_t idx, int owntc);
thdcap_t capmgr_initaep_create_cserialized(u32_t *sndtidret, u32_t *rcvtcret, spdid_t s, int owntc);
thdcap_t capmgr_thd_retrieve_next_cserialized(thdid_t *tid, int *unused, spdid_t s);

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
capmgr_thd_create(cos_thd_fn_t fn, void *data, thdid_t *tid)
{
	int unused;
	thdclosure_index_t idx = cos_thd_init_alloc(fn, data);

	if (idx < 1) return 0;

	return capmgr_thd_create_cserialized(tid, &unused, idx);
}

thdcap_t
capmgr_ext_thd_create(spdid_t child, thdclosure_index_t idx, thdid_t *tid)
{
	int unused;

	return capmgr_ext_thd_create_cserialized(tid, &unused, child, idx);
}

thdcap_t
capmgr_aep_create(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data, int owntc)
{
	u32_t tcrcvret = 0;
	thdcap_t thd = 0;
	arcvcap_t rcv = 0;
	tcap_t tc = 0;
	thdid_t tid = 0;
	thdclosure_index_t idx = cos_thd_init_alloc(cos_aepthd_fn, (void *)aep);

	if (idx < 1) return 0;

	thd = capmgr_aep_create_cserialized(&tid, &tcrcvret, idx, owntc);
	if (!thd) return 0;

	aep->fn   = fn;
	aep->data = data;
	aep->thd  = thd;
	aep->rcv  = (tcrcvret << 16) >> 16;
	aep->tc   = (tcrcvret >> 16);
	aep->tid  = tid;

	return aep->thd;
}

thdcap_t
capmgr_ext_aep_create(spdid_t child, struct cos_aep_info *aep, thdclosure_index_t idx, int owntc, arcvcap_t *extrcv)
{
	u32_t drcvtidret = 0;
	u32_t tcrcvret = 0;
	thdid_t tid = 0;
	thdcap_t thd = 0;

	thd = capmgr_ext_aep_create_cserialized(&drcvtidret, &tcrcvret, child, idx, owntc);
	if (!thd) return thd;

	aep->fn   = NULL;
	aep->data = NULL;
	aep->thd  = thd;
	aep->tid  = (drcvtidret << 16) >> 16;
	aep->rcv  = tcrcvret >> 16;
	aep->tc   = (tcrcvret << 16) >> 16;
	*extrcv   = drcvtidret >> 16;

	return aep->thd;
}

thdcap_t
capmgr_initaep_create(spdid_t child, struct cos_aep_info *aep, int owntc, asndcap_t *snd)
{
	thdcap_t thd = 0;
	u32_t sndtidret = 0, rcvtcret = 0;

	thd = capmgr_initaep_create_cserialized(&sndtidret, &rcvtcret, child, owntc);
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

int memmgr_shared_page_allocn_cserialized(vaddr_t *pgaddr, int *unused, int num_pages);
int memmgr_shared_page_map_cserialized(vaddr_t *pgaddr, int *unused, int id);

vaddr_t
memmgr_heap_page_alloc(void)
{
	return memmgr_heap_page_allocn(1);
}

int
memmgr_shared_page_allocn(int num_pages, vaddr_t *pgaddr)
{
	int unused = 0;

	return memmgr_shared_page_allocn_cserialized(pgaddr, &unused, num_pages);
}

int
memmgr_shared_page_alloc(vaddr_t *pgaddr)
{
	return memmgr_shared_page_allocn(1, pgaddr);
}

int
memmgr_shared_page_map(int id, vaddr_t *pgaddr)
{
	int unused = 0;

	return memmgr_shared_page_map_cserialized(pgaddr, &unused, id);
}
