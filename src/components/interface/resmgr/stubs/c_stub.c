#include <memmgr.h>
#include <resmgr.h>
#include <cos_thd_init.h>
#include <cos_defkernel_api.h>

thdcap_t resmgr_initthd_create_intern(spdid_t c, spdid_t s, int u1, int u2, int *u3, int *u4);
thdcap_t resmgr_thd_create_intern(spdid_t c, int idx, int u1, int u2, int *u3, int *u4);
thdcap_t resmgr_ext_thd_create_intern(spdid_t c, spdid_t s, int idx, int u1, int *u2, int *u3);
thdcap_t resmgr_aep_create_intern(spdid_t c, int idx, int owntc, int u1, arcvcap_t *rcvret, tcap_t *tcret);
thdcap_t resmgr_ext_aep_create_intern(spdid_t c, spdid_t s, int idx, int owntc, arcvcap_t *rcvret, u32_t *rcvtcret);
thdcap_t resmgr_initaep_create_intern(spdid_t c, spdid_t s, int owntc, int u1, asndcap_t *sndret, u32_t *rcvtcret);
thdcap_t resmgr_thd_retrieve_intern(spdid_t c, spdid_t s, thdid_t t, int u1, int *u2, int *u3);
thdid_t  resmgr_thd_retrieve_next_intern(spdid_t c, spdid_t s, int u1, int u2, thdcap_t *t, int *u3);
asndcap_t resmgr_asnd_create_intern(spdid_t c, spdid_t s, thdid_t t, int u1, int *u2, int *u3);

thdid_t
resmgr_thd_retrieve_next(spdid_t c, spdid_t s, thdcap_t *t)
{
	int unused;

	return resmgr_thd_retrieve_next_intern(c, s, unused, unused, t, &unused);
}

thdcap_t
resmgr_thd_retrieve(spdid_t c, spdid_t s, thdid_t t)
{
	int unused;

	return resmgr_thd_retrieve_intern(c, s, t, unused, &unused, &unused);
}

thdcap_t
resmgr_initthd_create(spdid_t c, spdid_t s)
{
	int unused;

	return resmgr_initthd_create_intern(c, s, unused, unused, &unused, &unused);
}

thdcap_t
resmgr_thd_create(spdid_t c, cos_thd_fn_t fn, void *data)
{
	int unused;
	int idx = cos_thd_init_alloc(fn, data);

	if (idx < 1) assert(0);

	return resmgr_thd_create_intern(c, idx, unused, unused, &unused, &unused);
}

thdcap_t
resmgr_ext_thd_create(spdid_t c, spdid_t s, int idx)
{
	int unused;

	return resmgr_ext_thd_create_intern(c, s, idx, unused, &unused, &unused);
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
	int unused = 0;
	int idx = cos_thd_init_alloc(__resmgr_aep_fn, (void *)aep);
	int ret;
	arcvcap_t rcv;
	tcap_t tc;

	if (idx < 1) assert(0);

	ret = resmgr_aep_create_intern(c, idx, owntc, unused, &rcv, &tc);
	assert(ret > 0);

	aep->fn   = fn;
	aep->data = data;
	aep->thd  = ret;
	aep->rcv  = rcv;
	aep->tc   = tc;

	return aep->thd;
}

thdcap_t
resmgr_ext_aep_create(spdid_t c, spdid_t s, struct cos_aep_info *aep, int idx, int owntc, arcvcap_t *extrcv)
{
	int ret;
	u32_t tcrcvret;

	ret = resmgr_ext_aep_create_intern(c, s, idx, owntc, extrcv, &tcrcvret);
	assert(ret > 0);

	aep->fn   = NULL;
	aep->data = NULL;
	aep->thd  = ret;
	aep->rcv  = tcrcvret >> 16;
	aep->tc   = (tcrcvret << 16) >> 16;

	return aep->thd;
}

thdcap_t
resmgr_initaep_create(spdid_t c, spdid_t s, struct cos_aep_info *aep, int owntc, asndcap_t *snd)
{
	int unused = 0;
	int ret;
	u32_t r2 = 0, r3 = 0;

	ret = resmgr_initaep_create_intern(c, s, owntc, unused, snd, &r3);
	assert(ret > 0);

	aep->fn   = NULL;
	aep->data = NULL;
	aep->thd  = ret;
	aep->rcv  = r3 >> 16;
	aep->tc   = (r3 << 16) >> 16;

	/* initcaps are copied to INITXXX offsets in the dst component */
	return ret;
}

asndcap_t
resmgr_asnd_create(spdid_t c, spdid_t s, thdid_t t)
{
	int unused;

	return resmgr_asnd_create_intern(c, s, t, unused, &unused, &unused);
}

int memmgr_heap_page_allocn_intern(spdid_t c, unsigned int npgs, int u1, int u2, int *u3, int *u4);

int memmgr_shared_page_allocn_intern(spdid_t c, int num_pages, int u1, int u2, vaddr_t *pgaddr, int *u3);
int memmgr_shared_page_map_intern(spdid_t c, int id, int u1, int u2, vaddr_t *pgaddr, int *num_pages);
vaddr_t memmgr_shared_page_vaddr_intern(spdid_t c, int id, int u1, int u2, int *u3, int *u4);

vaddr_t
memmgr_heap_page_allocn(spdid_t c, unsigned int npgs)
{
	int unused;

	return memmgr_heap_page_allocn_intern(c, npgs, unused, unused, &unused, &unused);
}

vaddr_t
memmgr_heap_page_alloc(spdid_t c)
{
	return memmgr_heap_page_allocn(c, 1);
}

int
memmgr_shared_page_allocn(spdid_t c, int num_pages, vaddr_t *pgaddr)
{
	int unused;

	return memmgr_shared_page_allocn_intern(c, num_pages, unused, unused, pgaddr, &unused);
}

int
memmgr_shared_page_alloc(spdid_t c, vaddr_t *pgaddr)
{
	return memmgr_shared_page_allocn(c, 1, pgaddr);
}

int
memmgr_shared_page_map(spdid_t c, int id, vaddr_t *pgaddr, int *num_pages)
{
	int unused = 0;

	return memmgr_shared_page_map_intern(c, id, unused, unused, pgaddr, num_pages);
}

vaddr_t
memmgr_shared_page_vaddr(spdid_t c, int id)
{
	int unused;

	return memmgr_shared_page_vaddr_intern(c, id, unused, unused, &unused, &unused);
}
