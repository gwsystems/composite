#include <memmgr.h>
#include <capmgr.h>
#include <cos_thd_init.h>
#include <cos_defkernel_api.h>

u32_t capmgr_initthd_create_intern(spdid_t c, spdid_t s);
u32_t capmgr_thd_create_intern(spdid_t c, int idx);
u32_t capmgr_ext_thd_create_intern(spdid_t c, spdid_t s, int idx);
u32_t capmgr_aep_create_intern(arcvcap_t *rcvret, tcap_t *tcret, spdid_t c, int idx, int owntc);
u32_t capmgr_ext_aep_create_intern(arcvcap_t *rcvret, u32_t *rcvtcret, spdid_t c, spdid_t s, int idx, int owntc);
u32_t capmgr_initaep_create_intern(asndcap_t *sndret, u32_t *rcvtcret, spdid_t c, spdid_t s, int owntc);
u32_t capmgr_thd_retrieve_intern(spdid_t c, spdid_t s, thdid_t t);
u32_t capmgr_thd_retrieve_next_intern(spdid_t c, spdid_t s);
asndcap_t capmgr_asnd_create_intern(spdid_t c, spdid_t s, thdid_t t);

#define __THDID_THDCAP(val, tid, thd) do { (tid) = ((val) >> 16); (thd) = ((val) << 16) >> 16; } while (0);

thdcap_t
capmgr_thd_retrieve_next(spdid_t child, thdid_t *tid)
{
	u32_t ret;
	thdcap_t thd;

	ret = capmgr_thd_retrieve_next_intern(0, child);
	if (!ret) return ret;
	__THDID_THDCAP(ret, *tid, thd);

	return thd;
}

thdcap_t
capmgr_thd_retrieve(spdid_t child, thdid_t t)
{
	return capmgr_thd_retrieve_intern(0, child, t);
}

thdcap_t
capmgr_initthd_create(spdid_t child, thdid_t *tid)
{
	u32_t ret;
	thdcap_t thd;

	ret = capmgr_initthd_create_intern(0, child);
	if (!ret) return ret;
	__THDID_THDCAP(ret, *tid, thd);

	return thd;
}

thdcap_t
capmgr_thd_create(cos_thd_fn_t fn, void *data, thdid_t *tid)
{
	u32_t ret;
	thdcap_t thd;
	int idx = cos_thd_init_alloc(fn, data);

	if (idx < 1) assert(0);

	ret = capmgr_thd_create_intern(0, idx);
	if (!ret) return ret;
	__THDID_THDCAP(ret, *tid, thd);

	return thd;
}

thdcap_t
capmgr_ext_thd_create(spdid_t child, int idx, thdid_t *tid)
{
	u32_t ret;
	thdcap_t thd;

	ret = capmgr_ext_thd_create_intern(0, child, idx);
	if (!ret) return ret;
	__THDID_THDCAP(ret, *tid, thd);

	return thd;
}

static void
__capmgr_aep_fn(void *data)
{
	struct cos_aep_info *ai    = (struct cos_aep_info *)data;
	cos_aepthd_fn_t      fn    = ai->fn;
	void *               fdata = ai->data;

	(fn)(ai->rcv, fdata);
}

thdcap_t
capmgr_aep_create(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data, int owntc)
{
	u32_t ret;
	thdcap_t thd;
	arcvcap_t rcv;
	tcap_t tc;
	thdid_t tid;
	int idx = cos_thd_init_alloc(__capmgr_aep_fn, (void *)aep);

	if (idx < 1) return 0;

	ret = capmgr_aep_create_intern(&rcv, &tc, 0, idx, owntc);
	if (!ret) return ret;
	__THDID_THDCAP(ret, tid, thd);

	aep->fn   = fn;
	aep->data = data;
	aep->thd  = thd;
	aep->rcv  = rcv;
	aep->tc   = tc;
	aep->tid  = tid;

	return aep->thd;
}

thdcap_t
capmgr_ext_aep_create(spdid_t child, struct cos_aep_info *aep, int idx, int owntc, arcvcap_t *extrcv)
{
	u32_t ret;
	u32_t tcrcvret;
	thdid_t tid;
	thdcap_t thd;

	ret = capmgr_ext_aep_create_intern(extrcv, &tcrcvret, 0, child, idx, owntc);
	if (!ret) return ret;
	__THDID_THDCAP(ret, tid, thd);

	aep->fn   = NULL;
	aep->data = NULL;
	aep->thd  = thd;
	aep->tid  = tid;
	aep->rcv  = tcrcvret >> 16;
	aep->tc   = (tcrcvret << 16) >> 16;

	return aep->thd;
}

thdcap_t
capmgr_initaep_create(spdid_t child, struct cos_aep_info *aep, int owntc, asndcap_t *snd)
{
	thdid_t tid;
	thdcap_t thd;
	u32_t ret;
	u32_t r2 = 0, r3 = 0;

	ret = capmgr_initaep_create_intern(snd, &r3, 0, child, owntc);
	if (!ret) return ret;
	__THDID_THDCAP(ret, tid, thd);

	aep->fn   = NULL;
	aep->data = NULL;
	aep->thd  = thd;
	aep->tid  = tid;
	aep->rcv  = r3 >> 16;
	aep->tc   = (r3 << 16) >> 16;

	/* initcaps are copied to INITXXX offsets in the dst component */
	return ret;
}

asndcap_t
capmgr_asnd_create(spdid_t child, thdid_t t)
{
	return capmgr_asnd_create_intern(0, child, t);
}

vaddr_t memmgr_heap_page_allocn_intern(spdid_t c, unsigned int npgs);

int memmgr_shared_page_allocn_intern(vaddr_t *pgaddr, int *unused, spdid_t c, int num_pages);
int memmgr_shared_page_map_intern(vaddr_t *pgaddr, int *unused, spdid_t c, int id);

vaddr_t
memmgr_heap_page_allocn(unsigned int npgs)
{
	return memmgr_heap_page_allocn_intern(0, npgs);
}

vaddr_t
memmgr_heap_page_alloc(void)
{
	return memmgr_heap_page_allocn(1);
}

int
memmgr_shared_page_allocn(int num_pages, vaddr_t *pgaddr)
{
	int unused = 0;

	return memmgr_shared_page_allocn_intern(pgaddr, &unused, 0, num_pages);
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

	return memmgr_shared_page_map_intern(pgaddr, &unused, 0, id);
}
