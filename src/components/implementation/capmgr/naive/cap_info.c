#include <cos_kernel_api.h>
#include <cap_info.h>

static struct cap_comp_info capci[MAX_NUM_COMPS + 1]; /* includes booter information also, so +1 */
static unsigned int cap_comp_count;
u32_t cap_info_schedbmp[NUM_CPU][MAX_NUM_COMP_WORDS];
static struct cap_shmem_glb_info cap_shmglbinfo;

static inline struct cap_shmem_glb_info *
__cap_info_shmglb_info(void)
{
	return &cap_shmglbinfo;
}

struct cap_comp_info *
cap_info_comp_find(spdid_t spdid)
{
	return &capci[spdid];
}

unsigned int
cap_info_count(void)
{
	return cap_comp_count;
}

struct sl_thd *
cap_info_thd_find(struct cap_comp_info *rci, thdid_t tid)
{
	int i;
	struct cap_comp_cpu_info *rci_cpu = NULL;

	if (!rci || !cap_info_init_check(rci)) return NULL;

	rci_cpu = cap_info_cpu_local(rci);
	for (i = 0; i < rci_cpu->thd_used; i++) {
		if (sl_thd_thdid(rci_cpu->thdinfo[i]) == tid) return rci_cpu->thdinfo[i];
	}

	return NULL;
}

struct sl_thd *
cap_info_thd_next(struct cap_comp_info *rci)
{
	struct cap_comp_cpu_info *rci_cpu = NULL;

	if (!rci || !cap_info_init_check(rci)) return NULL;
	rci_cpu = cap_info_cpu_local(rci);
	if (rci_cpu->p_thd_iterator < rci_cpu->thd_used) {
		return (rci_cpu->thdinfo[ps_faa((long unsigned *)&(rci_cpu->p_thd_iterator), 1)]);
	}

	return NULL;
}

struct cap_comp_info *
cap_info_comp_init(spdid_t spdid, captblcap_t captbl_cap, pgtblcap_t pgtbl_cap, compcap_t compcap,
		   capid_t cap_frontier, vaddr_t heap_frontier, spdid_t sched_spdid)
{
	struct cos_compinfo       *ci      = cos_compinfo_get(&(capci[spdid].defci));
	struct cap_shmem_info     *cap_shi = cap_info_shmem_info(&capci[spdid]);
	struct cap_shmem_glb_info *rglb    = __cap_info_shmglb_info();
	struct cap_comp_cpu_info  *rci_cpu = cap_info_cpu_local(&capci[spdid]);

	rci_cpu->thd_used = 1;
	rci_cpu->parent   = &capci[sched_spdid];

	capci[spdid].cid = spdid;
	cos_meminfo_init(&ci->mi, 0, 0, 0);
	cos_compinfo_init(ci, pgtbl_cap, captbl_cap, compcap, heap_frontier, cap_frontier,
			cos_compinfo_get(cos_defcompinfo_curr_get()));

	memset(rglb, 0, sizeof(struct cap_shmem_glb_info));
	memset(cap_shi, 0, sizeof(struct cap_shmem_info));
	cap_shi->cinfo = ci;

	capci[spdid].initflag = 1;
	ps_faa((unsigned long *)&cap_comp_count, 1);

	return &capci[spdid];
}

struct cap_channelaep_info *
cap_channelaep_get(cos_channelkey_t key)
{
	if (key == 0) return NULL;

	return &cap_channelaeps[key - 1];
}

void
cap_channelaep_set(cos_channelkey_t key, struct sl_thd *t)
{
	struct cap_channelaep_info *ak = cap_channelaep_get(key);

	if (!ak || ak->rcvcap) return;
	ak->rcvcap              = sl_thd_rcvcap(t);
	ak->sndcap[cos_cpuid()] = 0;
}

asndcap_t
cap_channelaep_asnd_get(cos_channelkey_t key)
{
	struct cos_compinfo        *cap_ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct cap_channelaep_info *ak     = cap_channelaep_get(key);

	if (!ak) return 0;
	if (ak->sndcap[cos_cpuid()]) return ak->sndcap[cos_cpuid()];
	ak->sndcap[cos_cpuid()] = cos_asnd_alloc(cap_ci, ak->rcvcap, cap_ci->captbl_cap);
	assert(ak->sndcap[cos_cpuid()]);

	return ak->sndcap[cos_cpuid()];
}

struct sl_thd *
cap_info_thd_init(struct cap_comp_info *rci, struct sl_thd *t, cos_channelkey_t key)
{
	int off;
	struct cap_comp_cpu_info *rci_cpu = NULL;

	if (!rci || !cap_info_init_check(rci)) return NULL;
	rci_cpu = cap_info_cpu_local(rci);
	if (rci_cpu->thd_used >= CAP_INFO_MAX_THREADS) return NULL;
	if (!t) return NULL;

	off = ps_faa((long unsigned *)&(rci_cpu->thd_used), 1);
	rci_cpu->thdinfo[off] = t;
	cap_channelaep_set(key, t);

	return t;
}

struct sl_thd *
cap_info_initthd_init(struct cap_comp_info *rci, struct sl_thd *t, cos_channelkey_t key)
{
	struct cap_comp_cpu_info *rci_cpu = NULL;

	if (!rci || !cap_info_init_check(rci)) return NULL;
	rci_cpu = cap_info_cpu_local(rci);
	if (rci_cpu->thd_used >= CAP_INFO_MAX_THREADS) return NULL;
	if (!t) return NULL;

	rci_cpu->thdinfo[0] = t;
	cap_channelaep_set(key, t);

	return t;
}

struct sl_thd *
cap_info_initthd(struct cap_comp_info *rci)
{
	if (!rci) return NULL;

	return cap_info_cpu_local(rci)->thdinfo[0];
}

void
cap_info_init(void)
{
	cap_comp_count = 0;
	memset(cap_info_schedbmp, 0, sizeof(u32_t) * MAX_NUM_COMP_WORDS * NUM_CPU);
	memset(capci, 0, sizeof(struct cap_comp_info)*(MAX_NUM_COMPS+1));
	memset(cap_channelaeps, 0, sizeof(struct cap_channelaep_info) * CAPMGR_AEPKEYS_MAX);
}

static inline vaddr_t
__cap_info_shm_capmgr_vaddr(cbuf_t id)
{
	return capci[cos_spd_id()].shminfo.shm_addr[id - 1];
}

static inline void
__cap_info_shm_capmgr_vaddr_set(cbuf_t id, vaddr_t v)
{
	capci[cos_spd_id()].shminfo.shm_addr[id - 1] = v;
}

static int
__cap_cos_shared_page_mapn(struct cos_compinfo *rci, unsigned long num_pages, vaddr_t capvaddr, vaddr_t *compvaddr)
{
	struct cos_compinfo *cap_ci = cos_compinfo_get(cos_defcompinfo_curr_get());

	assert(capvaddr);
	if (!capvaddr) return -1;

	*compvaddr = cos_mem_aliasn(rci, cap_ci, capvaddr, num_pages * PAGE_SIZE);
	if (!*compvaddr) return -1;

	return 0;
}

static int
__cap_cos_shared_page_allocn(struct cos_compinfo *rci, unsigned long num_pages, vaddr_t *capvaddr, vaddr_t *compvaddr)
{
	struct cos_compinfo *cap_ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	vaddr_t              src_pg;

	*capvaddr = src_pg = (vaddr_t)cos_page_bump_allocn(cap_ci, num_pages * PAGE_SIZE);
	if (!(*capvaddr)) return -1;

	if (__cap_cos_shared_page_mapn(rci, num_pages, src_pg, compvaddr)) return -1;

	return 0;
}

int
cap_shmem_region_key_set(cbuf_t id, cos_channelkey_t key)
{
	struct cap_shmem_glb_info *rglb = __cap_info_shmglb_info();

	if (id > rglb->free_region_id) return -1;
	if (rglb->region_npages[id - 1]) return -1;

	if (!ps_cas((unsigned long *)&rglb->region_keys[id - 1], 0, key)) return -1;

	return 0;
}

cbuf_t
cap_shmem_region_find(cos_channelkey_t key)
{
	struct cap_shmem_glb_info *rglb = __cap_info_shmglb_info();
	cbuf_t id = 0;
	cbuf_t i, free = rglb->free_region_id;

	for (i = 1; i <= free; i++) {
		cos_channelkey_t *k = &rglb->region_keys[i - 1];
		if (ps_load((unsigned long *)k) == (unsigned long)key) {
			id = i;
			break;
		}
	}

	return id;
}

cbuf_t
cap_shmem_region_alloc(struct cap_shmem_info *rsh, cos_channelkey_t key, unsigned long num_pages)
{
	struct cos_compinfo       *rsh_ci    = cap_info_shmem_ci(rsh);
	struct cap_shmem_glb_info *rglb      = __cap_info_shmglb_info();
	int                        ret;
	cbuf_t                     alloc_id = 0, fid;
	vaddr_t                    cap_addr, comp_addr;

	if (!rsh) goto done;
	/* limits check */
	if ((rglb->total_pages + num_pages) * PAGE_SIZE > MEMMGR_MAX_SHMEM_SIZE) goto done;
	if ((rsh->total_pages + num_pages) * PAGE_SIZE > MEMMGR_COMP_MAX_SHMEM) goto done;
	fid = ps_faa((unsigned long *)&(rglb->free_region_id), 1);
	fid++;
	if (fid > MEMMGR_MAX_SHMEM_REGIONS) goto done;

	/* check id unused */
	if (__cap_info_shm_capmgr_vaddr(fid)) goto done;
	if (cap_shmem_region_vaddr(rsh, fid)) goto done;
	if (key && cap_shmem_region_key_set(fid, key)) goto done;

	rglb->region_npages[fid - 1] = num_pages;
	ps_faa(&(rglb->total_pages), num_pages);
	ps_faa(&(rsh->total_pages), num_pages);

	ret = __cap_cos_shared_page_allocn(rsh_ci, num_pages, &cap_addr, &comp_addr);
	if (ret) goto done;

	__cap_info_shm_capmgr_vaddr_set(fid, cap_addr);
	cap_shmem_region_vaddr_set(rsh, fid, comp_addr);
	alloc_id = fid;

done:
	return alloc_id;
}

cbuf_t
cap_shmem_region_map(struct cap_shmem_info *rsh, cbuf_t id, cos_channelkey_t key, unsigned long *num_pages)
{
	struct cos_compinfo       *rsh_ci    = cap_info_shmem_ci(rsh);
	struct cap_shmem_glb_info *rglb      = __cap_info_shmglb_info();
	vaddr_t                    cap_addr  = 0, comp_addr;
	unsigned long              npages    = 0;
	int                        ret       = -1;

	if (!rsh) return 0;
	if (key) id = cap_shmem_region_find(key);
	if (!id || id > MEMMGR_MAX_SHMEM_REGIONS) return 0;
	cap_addr  = __cap_info_shm_capmgr_vaddr(id);
	if (!cap_addr) return 0;
	npages = rglb->region_npages[id - 1];

	/* if already mapped in this component, just return the mapped shm, instead of an error! */
	if (cap_shmem_region_vaddr(rsh, id)) goto done;

	if ((rsh->total_pages + npages) * PAGE_SIZE > MEMMGR_COMP_MAX_SHMEM) return 0;
	ret = __cap_cos_shared_page_mapn(rsh_ci, npages, cap_addr, &comp_addr);
	if (ret) return 0;

	cap_shmem_region_vaddr_set(rsh, id, comp_addr);

done:
	*num_pages = npages;

	return id;
}

vaddr_t
cap_shmem_region_vaddr(struct cap_shmem_info *rsh, cbuf_t id)
{
	return rsh->shm_addr[id - 1];
}

void
cap_shmem_region_vaddr_set(struct cap_shmem_info *rsh, cbuf_t id, vaddr_t addr)
{
	rsh->shm_addr[id - 1] = addr;
}
