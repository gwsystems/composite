#include <cobj_format.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_types.h>
#include <llprint.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <hypercall.h>
#include <res_spec.h>
#include <ps.h>
#include <bitmap.h>

/* Assembly function for sinv from new component */
extern word_t hypercall_entry_rets_inv(spdid_t cur, int op, word_t arg1, word_t arg2, word_t arg3, word_t *ret2, word_t *ret3);

extern int num_cobj;
extern spdid_t capmgr_spdid;
extern spdid_t root_spdid[];

struct cobj_header *hs[MAX_NUM_SPDS + 1];

struct comp_sched_info {
	comp_flag_t          flags;
	spdid_t              parent_spdid;
	u32_t                child_bitmap[MAX_NUM_COMP_WORDS]; /* bitmap of all child component spdids */
	unsigned int         num_child;                        /* number of child components */
	unsigned int         num_child_iter;                   /* iterator for getting child components */
} comp_schedinfo[NUM_CPU][MAX_NUM_SPDS + 1] CACHE_ALIGNED;

/* The booter uses this to keep track of each comp */
struct comp_cap_info {
	struct cos_defcompinfo  def_cinfo;
	struct usr_inv_cap      ST_user_caps[INTERFACE_UNDEF_SYMBS];
	vaddr_t                 vaddr_user_caps; /* vaddr of user caps table in comp */
	vaddr_t                 addr_start;
	vaddr_t                 vaddr_mapped_in_booter;
	vaddr_t                 upcall_entry;
	u32_t                   cpu_bitmap[NUM_CPU_BMP_WORDS];
	struct comp_sched_info *schedinfo[NUM_CPU];
} new_comp_cap_info[MAX_NUM_SPDS];

int                   schedule[NUM_CPU][MAX_NUM_SPDS];
volatile unsigned int sched_cur[NUM_CPU] = { 0 };

static inline struct comp_cap_info *
boot_spd_compcapinfo_get(spdid_t spdid)
{
	assert(spdid && spdid <= MAX_NUM_SPDS);

	return &(new_comp_cap_info[spdid-1]);
}

static inline struct comp_sched_info *
boot_spd_comp_schedinfo_curr_get(void)
{
	return &comp_schedinfo[cos_cpuid()][0];
}

static inline struct comp_sched_info *
boot_spd_comp_schedinfo_get(spdid_t spdid)
{
	if (spdid == 0) return boot_spd_comp_schedinfo_curr_get();

	assert(spdid <= MAX_NUM_SPDS);

	return (boot_spd_compcapinfo_get(spdid)->schedinfo)[cos_cpuid()];
}

static inline struct cos_defcompinfo *
boot_spd_defcompinfo_curr_get(void)
{
	return cos_defcompinfo_curr_get();
}

static inline struct cos_compinfo *
boot_spd_compinfo_curr_get(void)
{
	return cos_compinfo_get(boot_spd_defcompinfo_curr_get());
}

static inline struct cos_aep_info *
boot_spd_initaep_curr_get(void)
{
	return cos_sched_aep_get(boot_spd_defcompinfo_curr_get());
}

static inline struct cos_compinfo *
boot_spd_compinfo_get(spdid_t spdid)
{
	if (spdid == 0) return boot_spd_compinfo_curr_get();

	assert(spdid <= MAX_NUM_SPDS);

	return cos_compinfo_get(&(boot_spd_compcapinfo_get(spdid)->def_cinfo));
}

static inline struct cos_defcompinfo *
boot_spd_defcompinfo_get(spdid_t spdid)
{
	if (spdid == 0) return boot_spd_defcompinfo_curr_get();

	assert(spdid <= MAX_NUM_SPDS);

	return &(boot_spd_compcapinfo_get(spdid)->def_cinfo);
}

static inline struct cos_aep_info *
boot_spd_initaep_get(spdid_t spdid)
{
	if (spdid == 0) return boot_spd_initaep_curr_get();

	assert(spdid <= MAX_NUM_SPDS);

	return cos_sched_aep_get(boot_spd_defcompinfo_get(spdid));
}

static vaddr_t
boot_deps_map_sect(spdid_t spdid, vaddr_t *mapaddr)
{
	struct cos_compinfo *compinfo  = boot_spd_compinfo_get(spdid);
	struct cos_compinfo *boot_info = boot_spd_compinfo_curr_get();
	vaddr_t addr = (vaddr_t)cos_page_bump_alloc(boot_info);

	assert(addr);
	*mapaddr = cos_mem_alias(compinfo, boot_info, addr);
	if (*mapaddr == 0) BUG();

	return addr;
}

static void
boot_capmgr_mem_alloc(void)
{
	struct cos_compinfo *capmgr_info = boot_spd_compinfo_get(capmgr_spdid);
	struct cos_compinfo *boot_info   = boot_spd_compinfo_curr_get();
	unsigned long mem_sz;

	if (!capmgr_spdid) return;

	mem_sz = round_up_to_pgd_page(boot_info->mi.untyped_frontier - (boot_info->mi.untyped_ptr + LLBOOT_RESERVED_UNTYPED_SZ));
	assert(mem_sz >= CAPMGR_MIN_UNTYPED_SZ);
	PRINTLOG(PRINT_DEBUG, "Allocating %lu MB untyped memory to capability manager[=%u]\n", mem_sz/(1024*1024), capmgr_spdid);

	cos_meminfo_alloc(capmgr_info, BOOT_MEM_KM_BASE, mem_sz);
}

void
boot_comp_mem_alloc(spdid_t spdid)
{
	struct cos_compinfo *compinfo = boot_spd_compinfo_get(spdid);
	struct cos_compinfo *boot_info   = boot_spd_compinfo_curr_get();
	unsigned long mem_sz = capmgr_spdid ? CAPMGR_MIN_UNTYPED_SZ : LLBOOT_NEWCOMP_UNTYPED_SZ;

	if (capmgr_spdid) return;
	cos_meminfo_alloc(compinfo, BOOT_MEM_KM_BASE, mem_sz);
}

/* Initialize just the captblcap and pgtblcap, due to hack for upcall_fn addr */
static void
boot_compinfo_init(spdid_t spdid, captblcap_t *ct, pgtblcap_t *pt, u32_t heap_start_vaddr)
{
	struct cos_compinfo *compinfo  = boot_spd_compinfo_get(spdid);
	struct cos_compinfo *boot_info = boot_spd_compinfo_curr_get();

	*ct = cos_captbl_alloc(boot_info);
	assert(*ct);
	*pt = cos_pgtbl_alloc(boot_info);
	assert(*pt);

	cos_compinfo_init(compinfo, *pt, *ct, 0, (vaddr_t)heap_start_vaddr, BOOT_CAPTBL_FREE, boot_info);

	/*
	 * if this is a capmgr, let it manage its share (ideally rest of system memory) of memory.
	 * if there is no capmgr in the system, allow every component to manage its memory.
	 */
	if (!capmgr_spdid || (capmgr_spdid && spdid && spdid == capmgr_spdid)) {
		pgtblcap_t utpt;
		unsigned long mem_sz = capmgr_spdid ? CAPMGR_MIN_UNTYPED_SZ : LLBOOT_NEWCOMP_UNTYPED_SZ;

		utpt = cos_pgtbl_alloc(boot_info);
		assert(utpt);
		cos_meminfo_init(&(compinfo->mi), BOOT_MEM_KM_BASE, mem_sz, utpt);
	}
}

static void
boot_newcomp_sinv_alloc(spdid_t spdid)
{
	sinvcap_t sinv;
	int i = 0;
	int intr_spdid;
	void *user_cap_vaddr;
	struct cos_compinfo *interface_compinfo;
	struct cos_compinfo *compinfo  = boot_spd_compinfo_get(spdid);
	struct comp_cap_info *spdinfo  = boot_spd_compcapinfo_get(spdid);
	/* TODO: Purge rest of booter of spdid convention */
	invtoken_t token = (invtoken_t)spdid;

	/*
	 * Loop through all undefined symbs
	 */
	for (i = 0; i < INTERFACE_UNDEF_SYMBS; i++) {
		if (spdinfo->ST_user_caps[i].service_entry_inst > 0) {
			intr_spdid = spdinfo->ST_user_caps[i].invocation_count;
			assert(intr_spdid); /* booter interface not allowed */

			interface_compinfo = boot_spd_compinfo_get(intr_spdid);
			user_cap_vaddr = (void *) (spdinfo->vaddr_mapped_in_booter
						+ (spdinfo->vaddr_user_caps
						- spdinfo->addr_start) + (sizeof(struct usr_inv_cap) * i));

			/* Create sinv capability from client to server */
			sinv = cos_sinv_alloc(compinfo, interface_compinfo->comp_cap,
				(vaddr_t)spdinfo->ST_user_caps[i].service_entry_inst,
				token);

			assert(sinv > 0);

			spdinfo->ST_user_caps[i].cap_no = sinv;

			/*
			 * Now that we have the sinv allocated, we can copy in the symb user
			 * cap to correct index
			 */
			memcpy(user_cap_vaddr, &(spdinfo->ST_user_caps[i]),
					sizeof(struct usr_inv_cap));
		}
	}
}

/*
 * NOTE: This is code duplication! cos_defcompinfo_child_alloc() cannot be used here
 *       as init-threads are created only for booter's child components here.
 */
static void
boot_newcomp_defcinfo_init(spdid_t spdid)
{
	struct cos_defcompinfo *defci     = boot_spd_defcompinfo_curr_get();
	struct cos_aep_info    *sched_aep = boot_spd_initaep_curr_get();
	struct cos_aep_info    *child_aep = boot_spd_initaep_get(spdid);
	struct cos_compinfo    *child_ci  = boot_spd_compinfo_get(spdid);
	struct cos_compinfo    *boot_info = boot_spd_compinfo_curr_get();
	struct comp_sched_info *spdsi     = boot_spd_comp_schedinfo_get(spdid);

	child_aep->thd = cos_initthd_alloc(boot_info, child_ci->comp_cap);
	assert(child_aep->thd);

	if (spdsi->flags & COMP_FLAG_SCHED) {
		child_aep->tc = cos_tcap_alloc(boot_info);
		assert(child_aep->tc);

		child_aep->rcv = cos_arcv_alloc(boot_info, child_aep->thd, child_aep->tc, boot_info->comp_cap, sched_aep->rcv);
		assert(child_aep->rcv);
	} else {
		child_aep->tc  = sched_aep->tc;
		child_aep->rcv = sched_aep->rcv;
	}

	child_aep->fn   = NULL;
	child_aep->data = NULL;
}

static inline void
boot_comp_sched_set(spdid_t spdid)
{
	struct cos_aep_info *child_aep = boot_spd_initaep_get(spdid);
	int i = 0;

	/* capmgr init only on boot core! */
	if (!capmgr_spdid) goto set;
	/*
	 * if there is capmgr in the system, set it to be the first (index == 0) to initialize
	 */
	if (spdid == capmgr_spdid) goto done;
	i = 1;

set:
	while (schedule[cos_cpuid()][i] != 0) i++;
	assert(i < MAX_NUM_COMPS);

done:
	schedule[cos_cpuid()][i] = child_aep->thd;
}

static void
boot_sched_caps_init(spdid_t spdid)
{
	struct cos_compinfo    *boot_info = boot_spd_compinfo_curr_get();
	struct cos_compinfo    *ci        = boot_spd_compinfo_get(spdid);
	struct comp_sched_info *compsi    = boot_spd_comp_schedinfo_get(spdid);
	struct cos_aep_info    *child_aep = boot_spd_initaep_get(spdid);
	int ret, i;

	/* If booter should create the init caps in that component */
	if (compsi->parent_spdid) return;

	boot_newcomp_defcinfo_init(spdid);
	ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, boot_info, child_aep->thd);
	assert(ret == 0);

	if (compsi->flags & COMP_FLAG_SCHED) {
		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, boot_info, child_aep->rcv);
		assert(ret == 0);
		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, boot_info, child_aep->tc);
		assert(ret == 0);
	}

	boot_comp_sched_set(spdid);
}

static void
boot_newcomp_init_caps(spdid_t spdid)
{
	struct cos_compinfo    *boot_info = boot_spd_compinfo_curr_get();
	struct cos_compinfo    *ci        = boot_spd_compinfo_get(spdid);
	struct comp_sched_info *compsi    = boot_spd_comp_schedinfo_get(spdid);
	struct cos_aep_info    *child_aep = boot_spd_initaep_get(spdid);
	int ret, i;

	/*
	 * FIXME: Ideally only components with HW access should have this.
	 *       But copying to each component as cos_hw_cycles_per_usec() used to get cpu cycles uses this.
	 */
	ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITHW_BASE, boot_info, BOOT_CAPTBL_SELF_INITHW_BASE);
	assert(ret == 0);

	if (!capmgr_spdid || (compsi->flags & COMP_FLAG_SCHED)) {
		/*
		 * FIXME:
		 * This is an ugly hack to allow components to do cos_introspect()
		 * - to get thdid
		 * - to get budget on tcap
		 * - other introspect...requirements..
		 *
		 * I don't know a way to get away from this for now!
		 * If it were just thdid, capmgr could have returned the thdids!
		 */
		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_CT, boot_info, ci->captbl_cap);
		assert(ret == 0);
	}

	if (capmgr_spdid == spdid || !capmgr_spdid) {
		/*
		 * if this is a capmgr, let it manage resources.
		 * if there is no capmgr in the system, allow every component to manage its resources.
		 */
		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_PT, boot_info, ci->pgtbl_cap);
		assert(ret == 0);
		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_COMP, boot_info, ci->comp_cap);
		assert(ret == 0);
		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_UNTYPED_PT, boot_info, ci->mi.pgtbl_cap);
		assert(ret == 0);
	}

}

static void
boot_newcomp_create(spdid_t spdid, struct cos_compinfo *comp_info)
{
	struct cos_compinfo *compinfo  = boot_spd_compinfo_get(spdid);
	struct cos_compinfo *boot_info = boot_spd_compinfo_curr_get();
	struct comp_cap_info *spdinfo  = boot_spd_compcapinfo_get(spdid);
	captblcap_t ct = compinfo->captbl_cap;
	pgtblcap_t  pt = compinfo->pgtbl_cap;
	compcap_t   cc;
	sinvcap_t   sinv;
	thdcap_t    main_thd;
	int         i = 0;
	invtoken_t token = (invtoken_t)spdid;
	int ret;

	cc = cos_comp_alloc(boot_info, ct, pt, (vaddr_t)spdinfo->upcall_entry);
	assert(cc);
	compinfo->comp_cap = cc;

	/* Create sinv capability from Userspace to Booter components */
	sinv = cos_sinv_alloc(boot_info, boot_info->comp_cap, (vaddr_t)hypercall_entry_rets_inv, token);
	assert(sinv > 0);

	ret = cos_cap_cpy_at(compinfo, BOOT_CAPTBL_SINV_CAP, boot_info, sinv);
	assert(ret == 0);

	boot_newcomp_init_caps(spdid);
	boot_sched_caps_init(spdid);
}

static void
boot_bootcomp_init(void)
{
	static int first_time = 1;
	struct cos_compinfo    *boot_info = boot_spd_compinfo_curr_get();
	struct comp_sched_info *bootsi    = boot_spd_comp_schedinfo_curr_get();

	if (first_time) {
		first_time = 0;
		cos_meminfo_init(&(boot_info->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();
	} else {
		cos_defcompinfo_sched_init();
	}

	bootsi->flags |= COMP_FLAG_SCHED;
}

static void
boot_done(void)
{
	PRINTLOG(PRINT_DEBUG, "Booter: done creating system.\n");
	PRINTLOG(PRINT_DEBUG, "********************************\n");
	cos_thd_switch(schedule[cos_cpuid()][sched_cur[cos_cpuid()]]);
	PRINTLOG(PRINT_DEBUG, "Booter: done initializing child components.\n");
}

void
boot_root_sched_run(void)
{
	struct cos_aep_info *root_aep = NULL;
	int ret;

	if (!root_spdid[cos_cpuid()]) {
		PRINTLOG(PRINT_WARN, "No root scheduler!\n");
		return;
	}

	/* NOTE: Chronos delegations would replace this in some experiments! */
	root_aep = boot_spd_initaep_get(root_spdid[cos_cpuid()]);

	PRINTLOG(PRINT_DEBUG, "Root scheduler is %u, switching to it now!\n", root_spdid[cos_cpuid()]);
	ret = cos_tcap_transfer(root_aep->rcv, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, LLBOOT_ROOTSCHED_PRIO);
	assert(ret == 0);

	ret = cos_switch(root_aep->thd, root_aep->tc, LLBOOT_ROOTSCHED_PRIO, TCAP_TIME_NIL, 0, cos_sched_sync());
	PRINTLOG(PRINT_ERROR, "Root scheduler returned.\n");
	assert(0);
}

void
boot_thd_done(spdid_t c)
{
	struct comp_sched_info *si = boot_spd_comp_schedinfo_get(c);

	assert(si->parent_spdid == 0);
	ps_faa((long unsigned *)&sched_cur[cos_cpuid()], 1);

	PRINTLOG(PRINT_DEBUG, "Component %d initialized!\n", c);
	if (schedule[cos_cpuid()][sched_cur[cos_cpuid()]] != 0) {
		cos_thd_switch(schedule[cos_cpuid()][sched_cur[cos_cpuid()]]);
	} else {
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	}
}

static inline int
boot_comp_cap_cpy_at(spdid_t dstid, capid_t dstslot, spdid_t srcid, cap_t captype)
{
	struct cos_compinfo *bootci = boot_spd_compinfo_curr_get(), *dstci, *srcci;
	int ret = -EINVAL;

	if (srcid > num_cobj || dstid > num_cobj) goto done;
	if (!dstslot) goto done;

	dstci = boot_spd_compinfo_get(dstid);
	srcci = boot_spd_compinfo_get(srcid);
	switch(captype) {
	case CAP_CAPTBL:
	{
		ret = cos_cap_cpy_at(dstci, dstslot, bootci, srcci->captbl_cap);
		break;
	}
	case CAP_PGTBL:
	{
		ret = cos_cap_cpy_at(dstci, dstslot, bootci, srcci->pgtbl_cap);
		break;
	}
	case CAP_COMP:
	{
		ret = cos_cap_cpy_at(dstci, dstslot, bootci, srcci->comp_cap);
		break;
	}
	case CAP_THD:
	{
		struct cos_aep_info *initaep = boot_spd_initaep_get(srcid);

		if (!(initaep->thd)) goto done;
		ret = cos_cap_cpy_at(dstci, dstslot, bootci, initaep->thd);
		break;
	}
	case CAP_ARCV:
	{
		struct cos_aep_info *initaep = boot_spd_initaep_get(srcid);

		if (!(initaep->rcv)) goto done;
		ret = cos_cap_cpy_at(dstci, dstslot, bootci, initaep->rcv);
		break;
	}
	case CAP_TCAP:
	{
		struct cos_aep_info *initaep = boot_spd_initaep_get(srcid);

		if (!(initaep->tc)) goto done;
		ret = cos_cap_cpy_at(dstci, dstslot, bootci, initaep->tc);
		break;
	}
	default:
	{
		break;
	}
	}

done:
	return ret;
}

static inline int
boot_comp_initaep_get(spdid_t dstid, spdid_t srcid, thdcap_t thdslot, arcvcap_t rcvslot, tcap_t tcslot)
{
	struct comp_sched_info *si = NULL;
	int ret = -1;

	if (srcid > num_cobj || dstid > num_cobj) return -EINVAL;
	if (!thdslot) return -EINVAL;

	si = boot_spd_comp_schedinfo_get(srcid);
	if (si->flags & COMP_FLAG_SCHED && (!rcvslot || !tcslot)) return -EINVAL;

	ret = boot_comp_cap_cpy_at(dstid, thdslot, srcid, CAP_THD);
	if (ret) goto done;

	if (!(si->flags & COMP_FLAG_SCHED)) goto done;
	ret = boot_comp_cap_cpy_at(dstid, rcvslot, srcid, CAP_ARCV);
	if (ret) goto done;
	ret = boot_comp_cap_cpy_at(dstid, tcslot, srcid, CAP_TCAP);

done:
	return ret;
}

static inline int
boot_comp_info_get(spdid_t dstid, spdid_t srcid, pgtblcap_t ptslot, captblcap_t ctslot, compcap_t compslot, spdid_t *parentid)
{
	struct comp_sched_info *si = NULL;
	int ret = 0;

	if (srcid > num_cobj || dstid > num_cobj) return -EINVAL;
	if (!ptslot || !ctslot || !compslot) return -EINVAL;

	si = boot_spd_comp_schedinfo_get(srcid);

	ret = boot_comp_cap_cpy_at(dstid, ptslot, srcid, CAP_PGTBL);
	if (ret) goto done;
	ret = boot_comp_cap_cpy_at(dstid, ctslot, srcid, CAP_CAPTBL);
	if (ret) goto done;
	ret = boot_comp_cap_cpy_at(dstid, compslot, srcid, CAP_COMP);
	if (ret) goto done;
	*parentid = si->parent_spdid;

done:
	return ret;
}

static inline int
boot_comp_info_iter(spdid_t dstid, pgtblcap_t pgtslot, captblcap_t captslot, compcap_t ccslot, spdid_t *srcid, spdid_t *src_parentid)
{
	static int iter_idx = 0; /* including llbooter! */
	int remain = 0, ret = 0;

	if (iter_idx > num_cobj) return -1;

	*srcid = iter_idx;
	iter_idx++;
	/*remaining */
	remain = num_cobj + 1 - iter_idx;
	ret = boot_comp_info_get(dstid, *srcid, pgtslot, captslot, ccslot, src_parentid);
	if (ret) return -1;

	return remain;
}

static inline int
boot_comp_frontier_get(spdid_t dstid, spdid_t srcid, vaddr_t *vasfr, capid_t *capfr)
{
	struct cos_compinfo *a_ci = boot_spd_compinfo_get(srcid);

	assert(a_ci);
	*vasfr = a_ci->vas_frontier;
	*capfr = a_ci->cap_frontier;

	return 0;
}

static inline int
boot_comp_cpubitmap_get(spdid_t dstid, u32_t *lo, u32_t *hi)
{
	struct comp_cap_info *ci = boot_spd_compcapinfo_get(dstid);

	if (dstid > num_cobj) return -EINVAL;

	assert(NUM_CPU_BMP_WORDS <= 2);

	*lo = ci->cpu_bitmap[0];
	if (NUM_CPU_BMP_WORDS == 2) *hi = ci->cpu_bitmap[1];

	return 0;
}

static inline int
boot_comp_child_next(spdid_t dstid, spdid_t srcid, spdid_t *child, comp_flag_t *flag)
{
	struct comp_sched_info *si = boot_spd_comp_schedinfo_get(srcid), *sch = NULL;
	int i = 0, iter = -1, childid = 0, remaining;

	if (si->num_child == 0) return -1;

	for (i = 0; i <= MAX_NUM_SPDS; i++) {
		if (bitmap_check(si->child_bitmap, i)) {
			childid = i + 1;
			iter++;
		}

		if (iter >= 0 && (unsigned int)iter == si->num_child_iter) break;
	}
	if (i > MAX_NUM_SPDS || iter < 0 || (unsigned int)iter > si->num_child_iter) return -EACCES;

	si->num_child_iter++;
	*child = childid;
	sch    = boot_spd_comp_schedinfo_get(*child);
	*flag  = sch->flags;

	remaining = si->num_child - si->num_child_iter;
	if (si->num_child_iter == si->num_child) si->num_child_iter = 0; /* reset iterator */

	return remaining;
}

static inline int
__hypercall_resource_access_check(spdid_t dstid, spdid_t srcid, int capmgr_ignore)
{
	struct comp_sched_info *dstinfo = NULL;

	if (dstid > num_cobj || srcid > num_cobj) return 0;
	dstinfo = boot_spd_comp_schedinfo_get(dstid);

	/* capability manager if it exists is the only component allowed to access all resources */
	if (!capmgr_ignore)             return (capmgr_spdid && dstid == capmgr_spdid);
	else if (dstid == capmgr_spdid) return 1;
	/* if there is no capability manager, components are allowed to access their or resources of their child components */
	else                            return (srcid && (dstid == srcid ||
							  (bitmap_check(dstinfo->child_bitmap, srcid - 1))));
}

word_t
hypercall_entry(word_t *ret2, word_t *ret3, int op, word_t arg3, word_t arg4, word_t arg5)
{
	int ret1 = 0;
	spdid_t client = cos_inv_token();

	if (!client) return -EINVAL;

	switch(op) {
	case HYPERCALL_COMP_INIT_DONE:
	{
		boot_thd_done(client);
		break;
	}
	case HYPERCALL_COMP_INFO_GET:
	{
		pgtblcap_t  ptslot   = arg4 >> 16;
		captblcap_t ctslot   = (arg4 << 16) >> 16;
		compcap_t   compslot = (arg3 << 16) >> 16;
		spdid_t     srcid    = arg3 >> 16;

		if (!__hypercall_resource_access_check(client, srcid, 0)) return -EACCES;
		ret1 = boot_comp_info_get(client, srcid, ptslot, ctslot, compslot, (spdid_t *)ret2);

		break;
	}
	case HYPERCALL_COMP_INFO_NEXT:
	{
		pgtblcap_t  ptslot   = arg4 >> 16;
		captblcap_t ctslot   = (arg4 << 16) >> 16;
		compcap_t   compslot = (arg3 << 16) >> 16;

		if (!__hypercall_resource_access_check(client, 0, 0)) return -EACCES;
		ret1 = boot_comp_info_iter(client, ptslot, ctslot, compslot, (spdid_t *)ret2, (spdid_t *)ret3);

		break;
	}
	case HYPERCALL_COMP_FRONTIER_GET:
	{
		vaddr_t vasfr;
		capid_t capfr;
		spdid_t srcid = arg3;

		if (!__hypercall_resource_access_check(client, srcid, 1)) return -EACCES;
		ret1  = boot_comp_frontier_get(client, srcid, &vasfr, &capfr);
		if (ret1) goto done;

		*ret2 = vasfr;
		*ret3 = capfr;

		break;
	}
	case HYPERCALL_COMP_INITAEP_GET:
	{
		spdid_t   srcid   = arg3 >> 16;
		thdcap_t  thdslot = (arg3 << 16) >> 16;
		tcap_t    tcslot  = (arg4 << 16) >> 16;;
		arcvcap_t rcvslot = arg4 >> 16;

		if (!__hypercall_resource_access_check(client, srcid, 0)) return -EACCES;
		ret1 = boot_comp_initaep_get(client, srcid, thdslot, rcvslot, tcslot);

		break;
	}
	case HYPERCALL_COMP_CHILD_NEXT:
	{
		spdid_t srcid = arg3, child;
		comp_flag_t flags;

		if (!__hypercall_resource_access_check(client, srcid, 1)) return -EACCES;
		ret1 = boot_comp_child_next(client, srcid, &child, &flags);
		if (ret1 < 0) goto done;

		*ret2 = (word_t)child;
		*ret3 = (word_t)flags;

		break;
	}
	case HYPERCALL_COMP_COMPCAP_GET:
	{
		spdid_t   srcid    = arg3;
		compcap_t compslot = arg4;

		if (!__hypercall_resource_access_check(client, srcid, 1)) return -EACCES;
		ret1 = boot_comp_cap_cpy_at(client, compslot, srcid, CAP_COMP);

		break;
	}
	case HYPERCALL_COMP_CAPTBLCAP_GET:
	{
		spdid_t     srcid  = arg3;
		captblcap_t ctslot = arg4;

		if (!__hypercall_resource_access_check(client, srcid, 1)) return -EACCES;
		ret1 = boot_comp_cap_cpy_at(client, ctslot, srcid, CAP_CAPTBL);

		break;
	}
	case HYPERCALL_COMP_PGTBLCAP_GET:
	{
		spdid_t    srcid  = arg3;
		pgtblcap_t ptslot = arg4;

		if (!__hypercall_resource_access_check(client, srcid, 1)) return -EACCES;
		ret1 = boot_comp_cap_cpy_at(client, ptslot, srcid, CAP_PGTBL);

		break;
	}
	case HYPERCALL_COMP_CAPFRONTIER_GET:
	{
		vaddr_t vasfr;
		capid_t capfr;
		spdid_t srcid = arg3;

		if (!__hypercall_resource_access_check(client, srcid, 1)) return -EACCES;
		ret1  = boot_comp_frontier_get(client, srcid, &vasfr, &capfr);
		if (ret1) goto done;

		*ret2 = vasfr;

		break;
	}
	case HYPERCALL_COMP_CPUBITMAP_GET:
	{
		spdid_t srcid = arg3;

		if (!__hypercall_resource_access_check(client, srcid, 1)) return -EACCES;
		ret1 = boot_comp_cpubitmap_get(srcid, (u32_t *)ret2, (u32_t *)ret3);

		break;
	}
	case HYPERCALL_NUMCOMPS_GET:
	{
		ret1 = num_cobj + 1; /* including booter */
		break;
	}
	default:
	{
		return -EINVAL;
	}
	}

done:
	return ret1;
}
