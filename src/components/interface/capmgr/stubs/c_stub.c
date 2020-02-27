/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <memmgr.h>
#include <capmgr.h>
#include <cos_thd_init.h>
#include <cos_defkernel_api.h>

thdcap_t capmgr_initthd_create_cserialized(thdid_t *tid, int *unused, spdid_t s);
thdcap_t capmgr_initaep_create_cserialized(u32_t *sndtidret, u32_t *rcvtcret, u32_t spdid_owntc, u32_t key_ipimax, u32_t ipiwin32b);
thdcap_t capmgr_thd_create_cserialized(thdid_t *tid, int *unused, thdclosure_index_t idx);
thdcap_t capmgr_aep_create_ext_cserialized(u32_t *drcvtidret, u32_t *rcvtcret, u32_t spdid_owntc_thdidx, u32_t chkey_ipimax, u32_t ipiwin32b);
thdcap_t capmgr_thd_create_ext_cserialized(thdid_t *tid, int *unused, spdid_t s, thdclosure_index_t idx);
thdcap_t capmgr_aep_create_cserialized(thdid_t *tid, u32_t *tcrcvret, u32_t owntc_tidx, u32_t key_ipimax, u32_t ipiwin32b);
thdcap_t capmgr_thd_retrieve_next_cserialized(thdid_t *tid, int *unused, spdid_t s);
thdcap_t capmgr_thd_retrieve_cserialized(thdid_t *inittid, int *unused, spdid_t s, thdid_t tid);
arcvcap_t capmgr_rcv_create_cserialized(u32_t spd_tid, u32_t key_ipimax, u32_t ipiwin32b);

arcvcap_t
capmgr_rcv_create(spdid_t child, thdid_t tid, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax)
{
	u32_t spd_tid = (child << 16) | tid;
	u32_t key_ipimax = (key << 16) | ipimax;
	u32_t ipiwin32b = (u32_t)ipiwin;

	return capmgr_rcv_create_cserialized(spd_tid, key_ipimax, ipiwin32b);
}

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
capmgr_thd_create(cos_thd_fn_t fn, void *data, thdid_t *tid)
{
	int unused;
	thdclosure_index_t idx = cos_thd_init_alloc(fn, data);

	if (idx < 1) return 0;

	return capmgr_thd_create_cserialized(tid, &unused, idx);
}

thdcap_t
capmgr_thd_create_ext(spdid_t child, thdclosure_index_t idx, thdid_t *tid)
{
	int unused;

	return capmgr_thd_create_ext_cserialized(tid, &unused, child, idx);
}

thdcap_t
capmgr_aep_create(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax)
{
	u32_t tcrcvret = 0;
	thdcap_t thd = 0;
	arcvcap_t rcv = 0;
	tcap_t tc = 0;
	thdid_t tid = 0;
	thdclosure_index_t idx = cos_thd_init_alloc(cos_aepthd_fn, (void *)aep);
	u32_t owntc_idx = (owntc << 16) | idx;
	u32_t key_ipimax = (key << 16) | ((ipimax << 16) >> 16);
	u32_t ipiwin32b = (u32_t)ipiwin;

	if (idx < 1) return 0;

	thd = capmgr_aep_create_cserialized(&tid, &tcrcvret, owntc_idx, key_ipimax, ipiwin32b);
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
capmgr_aep_create_ext(spdid_t child, struct cos_aep_info *aep, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *extrcv)
{
	u32_t drcvtidret = 0;
	u32_t tcrcvret = 0;
	thdid_t tid = 0;
	thdcap_t thd = 0;
	u32_t owntc_spdid_thdidx = (owntc << 31) | (((child << 17) >> 17) << 16) | ((idx << 16) >> 16);
	u32_t key_ipimax = (key << 16) | ((ipimax << 16) >> 16);
	u32_t ipiwin32b  = (u32_t)ipiwin;

	thd = capmgr_aep_create_ext_cserialized(&drcvtidret, &tcrcvret, owntc_spdid_thdidx, key_ipimax, ipiwin32b);
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
capmgr_initaep_create(spdid_t child, struct cos_aep_info *aep, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, asndcap_t *snd)
{
	u32_t child_owntc = (child << 16) | owntc;
	u32_t key_ipimax  = (key << 16) >> ipimax;
	u32_t ipiwin32b   = (u32_t)ipiwin;
	thdcap_t thd = 0;
	u32_t sndtidret = 0, rcvtcret = 0;

	thd = capmgr_initaep_create_cserialized(&sndtidret, &rcvtcret, child_owntc, key_ipimax, ipiwin32b);
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
