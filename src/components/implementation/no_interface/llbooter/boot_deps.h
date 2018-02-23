#include <cobj_format.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_types.h>
#include <llprint.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <hypercall.h>
#include <res_spec.h>

#define UNDEF_SYMBS 64
#define CMP_UTYPMEM_SZ (1 << 27)
#define THD_PRIO 5
#define THD_PERIOD 10
#define THD_BUDGET 5
#define INIT_THDS_SIZE MAX_NUM_SPDS + 1

int num_cobj;

/* Assembly function for sinv from new component */
extern void *hypercall_entry_inv(spdid_t cur, int op, int arg1, int arg2, int *ret2, int *ret3);

extern int num_cobj;
extern int resmgr_spdid;
extern int root_spdid;
long boot_pgtbl_cap_transfer(int dst, int src, int cap_slot);
long boot_thd_cap_transfer(int dst, int src, int cap_slot);

typedef enum {
	BOOT_FLAG_SCHED = 1,
	BOOT_FLAG_MM    = 1<<1,
} boot_flag_t;

struct cobj_header *hs[MAX_NUM_SPDS + 1];

/* The booter uses this to keep track of each comp */
struct comp_cap_info {
	struct cos_defcompinfo def_cinfo;
	struct usr_inv_cap   ST_user_caps[UNDEF_SYMBS];
	vaddr_t              vaddr_user_caps; // vaddr of user caps table in comp
	vaddr_t              addr_start;
	vaddr_t              vaddr_mapped_in_booter;
	vaddr_t              upcall_entry;
	boot_flag_t          flags;
	spdid_t              parent_spdid;
	u64_t                childid_bitf;
	u64_t                childid_sched_bitf;
} new_comp_cap_info[MAX_NUM_SPDS + 1];

int                      schedule[MAX_NUM_SPDS + 1];
volatile size_t          sched_cur;

static inline struct cos_compinfo *
boot_spd_compinfo_get(spdid_t spdid)
{
	if (spdid == 0) return cos_compinfo_get(cos_defcompinfo_curr_get());

	return cos_compinfo_get(&(new_comp_cap_info[spdid].def_cinfo));
}

static inline struct cos_defcompinfo *
boot_spd_defcompinfo_get(spdid_t spdid)
{
	if (spdid == 0) return cos_defcompinfo_curr_get();

	return &(new_comp_cap_info[spdid].def_cinfo);
}

static inline struct cos_aep_info *
boot_spd_initaep_get(spdid_t spdid)
{
	if (spdid == 0) return cos_sched_aep_get(cos_defcompinfo_curr_get());

	return cos_sched_aep_get(boot_spd_defcompinfo_get(spdid));
}

static vaddr_t
boot_deps_map_sect(spdid_t spdid, vaddr_t dest_daddr)
{
	struct cos_compinfo *compinfo  = boot_spd_compinfo_get(spdid);
	struct cos_compinfo *boot_info = boot_spd_compinfo_get(0);
	vaddr_t addr = (vaddr_t)cos_page_bump_alloc(boot_info);

	assert(addr);
	if (cos_mem_alias_at(compinfo, dest_daddr, boot_info, addr)) BUG();
	cos_vasfrontier_init(compinfo, dest_daddr + PAGE_SIZE);

	return addr;
}

static void
boot_comp_pgtbl_expand(size_t n_pte, pgtblcap_t pt, vaddr_t vaddr, struct cobj_header *h)
{
	struct cos_compinfo *boot_info = boot_spd_compinfo_get(0);
	size_t i;
	int tot = 0;

	/* Expand Page table, could do this faster */
	for (i = 0; i < (size_t)h->nsect; i++) {
		tot += cobj_sect_size(h, i);
	}

	if (tot > SERVICE_SIZE) {
		n_pte = tot / SERVICE_SIZE;
		if (tot % SERVICE_SIZE) n_pte++;
	}

	for (i = 0; i < n_pte; i++, vaddr+=SERVICE_SIZE) {
		if (!cos_pgtbl_intern_alloc(boot_info, pt, vaddr, SERVICE_SIZE)) BUG();
	}
}

#define RESMGR_UNTYPED_MEM_SZ (COS_MEM_KERN_PA_SZ / 2)
#define BOOT_COMP_UNTYPED_SZ  (1<<24) /* 16MB */

/* Initialize just the captblcap and pgtblcap, due to hack for upcall_fn addr */
static void
boot_compinfo_init(spdid_t spdid, captblcap_t *ct, pgtblcap_t *pt, u32_t vaddr)
{
	struct cos_compinfo *compinfo  = boot_spd_compinfo_get(spdid);
	struct cos_compinfo *boot_info = boot_spd_compinfo_get(0);

	*ct = cos_captbl_alloc(boot_info);
	assert(*ct);
	*pt = cos_pgtbl_alloc(boot_info);
	assert(*pt);

	cos_compinfo_init(compinfo, *pt, *ct, 0, (vaddr_t)vaddr, BOOT_CAPTBL_FREE, boot_info);
	/*
	 * if this is a resmgr, let it manage its share (ideally rest of system memory) of memory.
	 * if there is no resmgr in the system, allow every component to manage its memory.
	 */
	if (resmgr_spdid) {
		if (spdid && spdid == resmgr_spdid) {
			pgtblcap_t utpt;

			utpt = cos_pgtbl_alloc(boot_info);
			assert(utpt);
			cos_meminfo_init(&(compinfo->mi), BOOT_MEM_KM_BASE, RESMGR_UNTYPED_MEM_SZ, utpt);
			cos_meminfo_alloc(compinfo, BOOT_MEM_KM_BASE, RESMGR_UNTYPED_MEM_SZ);
		}
	} else {
		pgtblcap_t utpt;

		utpt = cos_pgtbl_alloc(boot_info);
		assert(utpt);
		cos_meminfo_init(&(compinfo->mi), BOOT_MEM_KM_BASE, BOOT_COMP_UNTYPED_SZ, utpt);
		cos_meminfo_alloc(compinfo, BOOT_MEM_KM_BASE, BOOT_COMP_UNTYPED_SZ);
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
	/* TODO: Purge rest of booter of spdid convention */
	unsigned long token = (unsigned long)spdid;

	/*
	 * Loop through all undefined symbs
	 */
	for (i = 0; i < UNDEF_SYMBS; i++) {
		if ( new_comp_cap_info[spdid].ST_user_caps[i].service_entry_inst > 0) {

			intr_spdid = new_comp_cap_info[spdid].ST_user_caps[i].invocation_count;
			interface_compinfo = boot_spd_compinfo_get(intr_spdid);
			user_cap_vaddr = (void *) (new_comp_cap_info[spdid].vaddr_mapped_in_booter
						+ (new_comp_cap_info[spdid].vaddr_user_caps
						- new_comp_cap_info[spdid].addr_start) + (sizeof(struct usr_inv_cap) * i));

			/* Create sinv capability from client to server */
			sinv = cos_sinv_alloc(compinfo, interface_compinfo->comp_cap,
				(vaddr_t)new_comp_cap_info[spdid].ST_user_caps[i].service_entry_inst,
				token);

			assert(sinv > 0);

			new_comp_cap_info[spdid].ST_user_caps[i].cap_no = sinv;

			/*
			 * Now that we have the sinv allocated, we can copy in the symb user
			 * cap to correct index
			 */
			memcpy(user_cap_vaddr, &new_comp_cap_info[spdid].ST_user_caps[i],
					sizeof(struct usr_inv_cap));
		}
	}
}

/* TODO: possible to cos_defcompinfo_child_alloc if we somehow move allocations to one place */
static void
boot_newcomp_defcinfo_init(spdid_t spdid)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info *   sched_aep = cos_sched_aep_get(defci);
	struct cos_aep_info *   child_aep = boot_spd_initaep_get(spdid);
	struct cos_compinfo *   child_ci  = boot_spd_compinfo_get(spdid);
	struct cos_compinfo *   boot_info = boot_spd_compinfo_get(0);

	child_aep->thd = cos_initthd_alloc(boot_info, child_ci->comp_cap);
	assert(child_aep->thd);

	if (new_comp_cap_info[spdid].flags & BOOT_FLAG_SCHED) {
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

	while (schedule[i] != 0) i ++;
	assert(i < MAX_NUM_COMPS);
	schedule[i] = child_aep->thd;
}

static void
boot_newcomp_init_caps(spdid_t spdid)
{
	struct cos_compinfo  *boot_info = boot_spd_compinfo_get(0);
	struct cos_compinfo  *ci        = boot_spd_compinfo_get(spdid);
	struct comp_cap_info *capci     = &new_comp_cap_info[spdid];
	struct cos_aep_info  *child_aep = boot_spd_initaep_get(spdid);
	int ret, i;
	
	/* FIXME: not everyone should have it. but for now, because getting cpu cycles uses this */
	ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITHW_BASE, boot_info, BOOT_CAPTBL_SELF_INITHW_BASE);
	assert(ret == 0);

	if (resmgr_spdid && (capci->flags & BOOT_FLAG_SCHED)) {
		/*
		 * FIXME:
		 * This is an ugly hack to allow components to do cos_introspect()
		 * - to get thdid
		 * - to get budget on tcap
		 * - other introspect...requirements..
		 *
		 * I don't know a way to get away from this for now! 
		 * If it were just thdid, resmgr could have returned the thdids!
		 */
		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_CT, boot_info, ci->captbl_cap);
		assert(ret == 0);
	}

	if (resmgr_spdid == spdid || !resmgr_spdid) {
		/*
		 * if this is a resmgr, let it manage resources.
		 * if there is no resmgr in the system, allow every component to manage its resources.
		 */
		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_PT, boot_info, ci->pgtbl_cap);
		assert(ret == 0);
		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_CT, boot_info, ci->captbl_cap);
		assert(ret == 0);
		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_COMP, boot_info, ci->comp_cap);
		assert(ret == 0);
		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_UNTYPED_PT, boot_info, ci->mi.pgtbl_cap);
		assert(ret == 0);
	}

	/* If booter should create the init caps in that component */
	if (capci->parent_spdid == 0) {
		boot_newcomp_defcinfo_init(spdid);

		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITTHD_BASE, boot_info, child_aep->thd);
		assert(ret == 0);

		if (capci->flags & BOOT_FLAG_SCHED) {
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITRCV_BASE, boot_info, child_aep->rcv);
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITTCAP_BASE, boot_info, child_aep->tc);
			assert(ret == 0);
		}

		boot_comp_sched_set(spdid);
	}
}

static void
boot_newcomp_create(spdid_t spdid, struct cos_compinfo *comp_info)
{
	struct cos_compinfo *compinfo  = boot_spd_compinfo_get(spdid);
	struct cos_compinfo *boot_info = boot_spd_compinfo_get(0);
	captblcap_t ct = compinfo->captbl_cap;
	pgtblcap_t  pt = compinfo->pgtbl_cap;
	compcap_t   cc;
	sinvcap_t   sinv;
	thdcap_t    main_thd;
	int         i = 0;
	unsigned long token = (unsigned long) spdid;
	int ret;

	cc = cos_comp_alloc(boot_info, ct, pt, (vaddr_t)new_comp_cap_info[spdid].upcall_entry);
	assert(cc);
	compinfo->comp_cap = cc;

	/* Create sinv capability from Userspace to Booter components */
	sinv = cos_sinv_alloc(boot_info, boot_info->comp_cap, (vaddr_t)hypercall_entry_inv, token);
	assert(sinv > 0);

	ret = cos_cap_cpy_at(compinfo, BOOT_CAPTBL_SINV_CAP, boot_info, sinv);
	assert(ret == 0);

	boot_newcomp_sinv_alloc(spdid);
	boot_newcomp_init_caps(spdid);
}

static void
boot_bootcomp_init(void)
{
	struct cos_compinfo *boot_info = boot_spd_compinfo_get(0);
	struct comp_cap_info *capci    = &new_comp_cap_info[0];

	/* TODO: if posix already did meminfo init */
	cos_meminfo_init(&(boot_info->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	capci->flags |= BOOT_FLAG_SCHED;
}

#define LLBOOT_ROOT_PRIO 1

static void
boot_done(void)
{
	struct cos_aep_info *root_aep = NULL;
	int ret;

	PRINTC("Booter: done creating system.\n");
	PRINTC("********************************\n");
	cos_thd_switch(schedule[sched_cur]);
	PRINTC("Booter: done initializing child components.\n");

	if (root_spdid) {
		/* NOTE: Chronos delegations would replace this in some  experiments! */
		root_aep = boot_spd_initaep_get(root_spdid);

		PRINTC("Root scheduler is %u, switching to it now!\n", root_spdid);
		ret = cos_tcap_transfer(root_aep->rcv, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_RES_INF, LLBOOT_ROOT_PRIO);
		assert(ret == 0);

		ret = cos_switch(root_aep->thd, root_aep->tc, LLBOOT_ROOT_PRIO, TCAP_TIME_NIL, 0, cos_sched_sync());
		PRINTC("ERROR: Root scheduler returned.\n");
		assert(0);
	}

	PRINTC("No root scheduler in the system. Spinning!\n");
	while (1) ;
}

void
boot_thd_done(spdid_t c)
{
	struct comp_cap_info *acomp = &new_comp_cap_info[c];

	assert(acomp->parent_spdid == 0);
	sched_cur++;

	PRINTC("Component %d initialized!\n", c);
	if (schedule[sched_cur] != 0) {
		cos_thd_switch(schedule[sched_cur]);
	} else {
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
}

static inline int
boot_comp_initthd_get(spdid_t dstid, spdid_t srcid, thdcap_t thdslot, arcvcap_t rcvslot, tcap_t tcslot)
{
	struct cos_compinfo  *boot_info = boot_spd_compinfo_get(0);
	struct comp_cap_info *acomp     = &new_comp_cap_info[srcid];
	struct cos_compinfo  *resci     = boot_spd_compinfo_get(dstid);
	struct cos_aep_info  *initaep   = boot_spd_initaep_get(srcid);
	int ret = 0;

	if (srcid > num_cobj || dstid > num_cobj) return -1;

	if (initaep->thd) {
		thdcap_t t;

		ret = cos_cap_cpy_at(resci, thdslot, boot_info, initaep->thd);
		assert(ret == 0);

		if (acomp->flags & BOOT_FLAG_SCHED) {
			assert(initaep->rcv && initaep->tc);
			ret = cos_cap_cpy_at(resci, rcvslot, boot_info, initaep->rcv);
			assert(ret == 0);

			ret = cos_cap_cpy_at(resci, tcslot, boot_info, initaep->tc);
			assert(ret == 0);
		}

		return 0;
	}

	return -1;
}

static int 
boot_comp_info_get(spdid_t dstid, spdid_t srcid, pgtblcap_t pgtslot, captblcap_t captslot, compcap_t ccslot, spdid_t *psid)
{
	struct cos_compinfo *boot_info = boot_spd_compinfo_get(0);
	struct cos_compinfo *a_ci, *resci;
	int ret = 0;

	if (srcid > num_cobj || dstid > num_cobj) return -1;

	a_ci  = boot_spd_compinfo_get(srcid);
	resci = boot_spd_compinfo_get(dstid);
	assert(a_ci);
	assert(resci);

	ret = cos_cap_cpy_at(resci, pgtslot, boot_info, a_ci->pgtbl_cap);
	assert(ret == 0);
	ret = cos_cap_cpy_at(resci, captslot, boot_info, a_ci->captbl_cap);
	assert(ret == 0);
	ret = cos_cap_cpy_at(resci, ccslot, boot_info, a_ci->comp_cap);
	assert(ret == 0);
	*psid = new_comp_cap_info[srcid].parent_spdid;

	return 0;
}

static int 
boot_comp_info_iter(spdid_t dstid, pgtblcap_t pgtslot, captblcap_t captslot, compcap_t ccslot, spdid_t *srcid, spdid_t *srcpsid)
{
	static int iter_idx = 0; /* including llbooter! */
	int remain = 0, ret = 0;

	if (iter_idx > num_cobj) return -1;

	*srcid = iter_idx;
	iter_idx ++;
	/*remaining */
	remain = num_cobj + 1 - iter_idx;
	ret = boot_comp_info_get(dstid, *srcid, pgtslot, captslot, ccslot, srcpsid);
	if (ret) return -1;

	return remain;
}

static int
boot_comp_frontier_get(spdid_t dstid, spdid_t srcid, vaddr_t *vasfr, capid_t *capfr)
{
	struct cos_compinfo *a_ci = boot_spd_compinfo_get(srcid);

	assert(a_ci);
	*vasfr = a_ci->vas_frontier;
	*capfr = a_ci->cap_frontier;

	return 0;
}

static int
boot_comp_childschedspds_get(spdid_t dstid, spdid_t srcid, u64_t *idbits)
{
	*idbits = new_comp_cap_info[srcid].childid_sched_bitf;

	return 0;
}

static int
boot_comp_childspds_get(spdid_t dstid, spdid_t srcid, u64_t *idbits)
{
	*idbits = new_comp_cap_info[srcid].childid_bitf;

	return 0;
}

u32_t
hypercall_entry(spdid_t curr, int op, u32_t arg3, u32_t arg4, u32_t *ret2, u32_t *ret3)
{
	int ret1 = 0;

	switch(op) {
	case HYPERCALL_COMP_INIT_DONE:
	{
		boot_thd_done(curr);
		break;
	}
	case HYPERCALL_COMP_INFO_GET:
	{
		pgtblcap_t  pgtslot  = arg4 >> 16;
		captblcap_t captslot = (arg4 << 16) >> 16;
		compcap_t   ccslot   = (arg3 << 16) >> 16;
		spdid_t     srcid    = arg3 >> 16;

		/* only resource manager is allowed to use this function */
		assert(curr == resmgr_spdid);

		ret1 = boot_comp_info_get(curr, srcid, pgtslot, captslot, ccslot, (spdid_t *)ret2);

		break;
	}
	case HYPERCALL_COMP_INFO_NEXT:
	{
		pgtblcap_t  pgtslot  = arg4 >> 16;
		captblcap_t captslot = (arg4 << 16) >> 16;
		compcap_t   ccslot   = (arg3 << 16) >> 16;

		/* only resource manager is allowed to use this function */
		assert(curr == resmgr_spdid);
		ret1 = boot_comp_info_iter(curr, pgtslot, captslot, ccslot, (spdid_t *)ret2, (spdid_t *)ret3);

		break;
	}
	case HYPERCALL_COMP_FRONTIER_GET:
	{
		vaddr_t vasfr;
		capid_t capfr;

		/* only resource manager is allowed to use this function */
		assert(curr == resmgr_spdid);
		ret1 = boot_comp_frontier_get(curr, arg3, &vasfr, &capfr);

		*ret2 = vasfr;
		*ret3 = capfr;

		break;
	}
	case HYPERCALL_COMP_INITTHD_GET:
	{
		spdid_t   srcid   = arg3 >> 16;
		thdcap_t  thdslot = (arg3 << 16) >> 16;
		tcap_t    tcslot  = (arg4 << 16) >> 16;;
		arcvcap_t rcvslot = arg4 >> 16;

		/* only resource manager is allowed to use this function */
		assert(curr == resmgr_spdid && srcid != resmgr_spdid);
		ret1 = boot_comp_initthd_get(curr, srcid, thdslot, rcvslot, tcslot);

		break;
	}
	case HYPERCALL_COMP_CHILDSPDIDS_GET:
	{
		u64_t idbits = 0;

		/* current componet is allowed to get it's info or resmgr is allowed to get any comp info */
		if (curr != resmgr_spdid) assert(curr == arg3);

		ret1  = boot_comp_childspds_get(curr, arg3, &idbits);
		*ret2 = (u32_t)idbits;
		*ret3 = (u32_t)(idbits >> 32);

		break;
	}
	case HYPERCALL_COMP_CHILDSCHEDSPDIDS_GET:
	{
		u64_t idbits = 0;

		/* current componet is allowed to get it's info or resmgr is allowed to get any comp info */
		if (curr != resmgr_spdid) assert(curr == arg3);

		ret1  = boot_comp_childschedspds_get(curr, arg3, &idbits);
		*ret2 = (u32_t)idbits;
		*ret3 = (u32_t)(idbits >> 32);

		break;
	}
	default:
	{
		assert(0);
	}
	}

	return ret1;
}
