#include <cobj_format.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_types.h>
#include <llprint.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <llboot.h>
#include <res_spec.h>

#define UNDEF_SYMBS 64

/* Assembly function for sinv from new component */
extern void *llboot_entry_inv(int a, int b, int c);
extern int num_cobj;
extern int resmgr_spdid;
extern int root_spdid;

struct cobj_header *hs[MAX_NUM_SPDS + 1];

/* The booter uses this to keep track of each comp */
struct comp_cap_info {
	struct cos_compinfo *compinfo;
	struct cos_defcompinfo *defci;

	struct cos_defcompinfo def_cinfo;
	struct usr_inv_cap   ST_user_caps[UNDEF_SYMBS];
	vaddr_t              vaddr_user_caps; // vaddr of user caps table in comp
	vaddr_t              addr_start;
	vaddr_t              vaddr_mapped_in_booter;
	vaddr_t              upcall_entry;
	int                  is_sched;
	int                  sched_no;
	int                  parent_no;
	spdid_t              parent_spdid;
	u64_t                childid_bitf;
	u64_t                childid_sched_bitf;
	struct sl_thd       *initaep;
} new_comp_cap_info[MAX_NUM_SPDS + 1];

struct cos_compinfo *boot_info;

int                      schedule[MAX_NUM_SPDS + 1];
volatile size_t          sched_cur;

static vaddr_t
boot_deps_map_sect(spdid_t spdid, vaddr_t dest_daddr)
{
	vaddr_t addr = (vaddr_t)cos_page_bump_alloc(boot_info);
	assert(addr);

	if (cos_mem_alias_at(new_comp_cap_info[spdid].compinfo, dest_daddr, boot_info, addr)) BUG();
	cos_vasfrontier_init(new_comp_cap_info[spdid].compinfo, dest_daddr + PAGE_SIZE);

	return addr;
}

static void
boot_comp_pgtbl_expand(size_t n_pte, pgtblcap_t pt, vaddr_t vaddr, struct cobj_header *h)
{
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

	for (i = 0; i < n_pte; i++) {
		if (!cos_pgtbl_intern_alloc(boot_info, pt, vaddr, SERVICE_SIZE)) BUG();
	}
}

#define RESMGR_UNTYPED_MEM_SZ (COS_MEM_KERN_PA_SZ - (4 * PGD_RANGE))

/* Initialize just the captblcap and pgtblcap, due to hack for upcall_fn addr */
static void
boot_compinfo_init(spdid_t spdid, captblcap_t *ct, pgtblcap_t *pt, u32_t vaddr)
{
	*ct = cos_captbl_alloc(boot_info);
	assert(*ct);
	*pt = cos_pgtbl_alloc(boot_info);
	assert(*pt);

	/* FIXME: some of the data-structures are redundant! can be removed! */
	new_comp_cap_info[spdid].defci = &new_comp_cap_info[spdid].def_cinfo;
	new_comp_cap_info[spdid].compinfo = cos_compinfo_get(new_comp_cap_info[spdid].defci);

	cos_compinfo_init(new_comp_cap_info[spdid].compinfo, *pt, *ct, 0, (vaddr_t)vaddr, BOOT_CAPTBL_FREE, boot_info);
	if (spdid && spdid == resmgr_spdid) {
		pgtblcap_t utpt;

		utpt = cos_pgtbl_alloc(boot_info);
		assert(utpt);
		cos_meminfo_init(&(new_comp_cap_info[spdid].compinfo->mi), BOOT_MEM_KM_BASE, RESMGR_UNTYPED_MEM_SZ, utpt);
		cos_meminfo_alloc(new_comp_cap_info[spdid].compinfo, BOOT_MEM_KM_BASE, RESMGR_UNTYPED_MEM_SZ);
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
	struct cos_compinfo *newcomp_compinfo = new_comp_cap_info[spdid].compinfo;
	/* TODO: Purge rest of booter of spdid convention */
	unsigned long token = (unsigned long)spdid;

	/*
	 * Loop through all undefined symbs
	 */
	for (i = 0; i < UNDEF_SYMBS; i++) {
		if ( new_comp_cap_info[spdid].ST_user_caps[i].service_entry_inst > 0) {

			intr_spdid = new_comp_cap_info[spdid].ST_user_caps[i].invocation_count;
			interface_compinfo = new_comp_cap_info[intr_spdid].compinfo;
			user_cap_vaddr = (void *) (new_comp_cap_info[spdid].vaddr_mapped_in_booter
						+ (new_comp_cap_info[spdid].vaddr_user_caps
						- new_comp_cap_info[spdid].addr_start) + (sizeof(struct usr_inv_cap) * i));

			/* Create sinv capability from client to server */
			sinv = cos_sinv_alloc(newcomp_compinfo, interface_compinfo->comp_cap,
					      (vaddr_t)new_comp_cap_info[spdid].ST_user_caps[i].service_entry_inst, token);
			assert(sinv > 0);

			new_comp_cap_info[spdid].ST_user_caps[i].cap_no = sinv;

			/* Now that we have the sinv allocated, we can copy in the symb user cap to correct index */
			memcpy(user_cap_vaddr, &new_comp_cap_info[spdid].ST_user_caps[i], sizeof(struct usr_inv_cap));
		}
	}
}

/* TODO: possible to cos_defcompinfo_child_alloc if we somehow move allocations to one place */
static void
boot_newcomp_defcinfo_init(spdid_t spdid)
{
	struct cos_defcompinfo *defci     = cos_defcompinfo_curr_get();
	struct cos_aep_info *   sched_aep = cos_sched_aep_get(defci);
	struct cos_aep_info *   child_aep = cos_sched_aep_get(new_comp_cap_info[spdid].defci);
	struct cos_compinfo *   child_ci  = cos_compinfo_get(new_comp_cap_info[spdid].defci);

	child_aep->thd = cos_initthd_alloc(boot_info, child_ci->comp_cap);
	assert(child_aep->thd);

	if (new_comp_cap_info[spdid].is_sched) {
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

static void
boot_newcomp_init_caps(spdid_t spdid)
{
	struct cos_compinfo  *ci    = new_comp_cap_info[spdid].compinfo;
	struct comp_cap_info *capci = &new_comp_cap_info[spdid];
	int ret, i;
	
	/* FIXME: not everyone should have it. but for now, because getting cpu cycles uses this */
	ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITHW_BASE, boot_info, BOOT_CAPTBL_SELF_INITHW_BASE);
	assert(ret == 0);

	if (capci->is_sched) {
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

	/* If booter should create the init caps in that component */
	if (capci->parent_spdid == 0) {
		boot_newcomp_defcinfo_init(spdid);
		capci->initaep = sl_thd_comp_init(capci->defci, capci->is_sched);
		assert(capci->initaep);

		/* TODO: Scheduling parameters to schedule them! */

		ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITTHD_BASE, boot_info, sl_thd_thdcap(capci->initaep));
		assert(ret == 0);

		if (capci->is_sched) {
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITRCV_BASE, boot_info, sl_thd_rcvcap(capci->initaep));
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_INITTCAP_BASE, boot_info, sl_thd_tcap(capci->initaep));
			assert(ret == 0);
		}

		if (resmgr_spdid == spdid) {
			assert(capci->is_sched == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_PT, boot_info, ci->pgtbl_cap);
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_CT, boot_info, ci->captbl_cap);
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_COMP, boot_info, ci->comp_cap);
			assert(ret == 0);
			ret = cos_cap_cpy_at(ci, BOOT_CAPTBL_SELF_UNTYPED_PT, boot_info, ci->mi.pgtbl_cap);
			assert(ret == 0);
		}

		/* FIXME: remove when llbooter can do something else for scheduling bootup phase */
		i = 0;
		while (schedule[i] != 0) i ++;
		schedule[i] = sl_thd_thdcap(capci->initaep);
	}
}

static void
boot_newcomp_create(spdid_t spdid, struct cos_compinfo *comp_info)
{
	compcap_t   cc;
	captblcap_t ct = new_comp_cap_info[spdid].compinfo->captbl_cap;
	pgtblcap_t  pt = new_comp_cap_info[spdid].compinfo->pgtbl_cap;
	sinvcap_t   sinv;
	thdcap_t    main_thd;
	int         i = 0;
	unsigned long token = (unsigned long) spdid;
	int ret;

	cc = cos_comp_alloc(boot_info, ct, pt, (vaddr_t)new_comp_cap_info[spdid].upcall_entry);
	assert(cc);
	new_comp_cap_info[spdid].compinfo->comp_cap = cc;

	/* Create sinv capability from Userspace to Booter components */
	sinv = cos_sinv_alloc(boot_info, boot_info->comp_cap, (vaddr_t)llboot_entry_inv, token);
	assert(sinv > 0);

	ret = cos_cap_cpy_at(new_comp_cap_info[spdid].compinfo, BOOT_CAPTBL_SINV_CAP, boot_info, sinv);
	assert(ret == 0);

	boot_newcomp_sinv_alloc(spdid);
	boot_newcomp_init_caps(spdid);
}

static void
boot_bootcomp_init(void)
{
	boot_info = cos_compinfo_get(cos_defcompinfo_curr_get());

	/* TODO: if posix already did meminfo init */
	cos_meminfo_init(&(boot_info->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	sl_init(SL_MIN_PERIOD_US);
}

#define LLBOOT_ROOT_PRIO 1
#define LLBOOT_ROOT_BUDGET_MS (10*1000)
#define LLBOOT_ROOT_PERIOD_MS (10*1000)

#undef LLBOOT_CHRONOS_ENABLED

static void
boot_done(void)
{
	struct sl_thd *root = NULL;
	int ret;

	printc("Booter: done creating system.\n");
	printc("********************************\n");
	cos_thd_switch(schedule[sched_cur]);

	if (root_spdid) {
		printc("Root scheduler is %u\n", root_spdid);
		root = new_comp_cap_info[root_spdid].initaep;
		assert(root);
		sl_thd_param_set(root, sched_param_pack(SCHEDP_PRIO, LLBOOT_ROOT_PRIO));
#ifdef LLBOOT_CHRONOS_ENABLED
		sl_thd_param_set(root, sched_param_pack(SCHEDP_BUDGET, LLBOOT_ROOT_BUDGET_MS));
		sl_thd_param_set(root, sched_param_pack(SCHEDP_WINDOW, LLBOOT_ROOT_PERIOD_MS));
#else
		ret = cos_tcap_transfer(sl_thd_rcvcap(root), sl__globals()->sched_tcap, TCAP_RES_INF, LLBOOT_ROOT_PRIO);
		assert(ret == 0);
#endif
	}

	printc("Starting llboot sched loop\n");
	sl_sched_loop();
}

/* Run after a componenet is done init execution, via sinv() into booter */
void
boot_thd_done(void)
{
	sched_cur++;

	if (schedule[sched_cur] != 0) {
		cos_thd_switch(schedule[sched_cur]);
	} else {
		printc("Done Initializing\n");
		printc("********************************\n");
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
}

/* assume capid_t is 16bit for packing */
#define BOOT_CI_GET_ERROR LLBOOT_ERROR

static thdcap_t
boot_comp_initthd_get(spdid_t spdid, capid_t *resfr)
{
	struct comp_cap_info *acomp = &new_comp_cap_info[spdid];
	struct cos_compinfo *resci = new_comp_cap_info[resmgr_spdid].compinfo;

	if (acomp->initaep && sl_thd_thdcap(acomp->initaep)) {
		thdcap_t t;

		t = cos_cap_cpy(resci, boot_info, CAP_THD, sl_thd_thdcap(acomp->initaep));
		assert(t);

		*resfr = resci->cap_frontier;
		return t;
	}

	*resfr = 0;
	return 0;
}

static int 
boot_comp_info_get(capid_t curresfr, spdid_t spdid, pgtblcap_t *pgc, captblcap_t *capc, compcap_t *cc, spdid_t *psid)
{
	struct cos_compinfo *a_ci, *resci;

	/* looks like the boot comps index start from 1 in that array */
	if (spdid > num_cobj) {
		return BOOT_CI_GET_ERROR;
	}

	a_ci  = new_comp_cap_info[spdid].compinfo;
	resci = new_comp_cap_info[resmgr_spdid].compinfo;
	assert(a_ci);
	assert(resci);

	cos_capfrontier_init(resci, curresfr);
	/* FIXME: need resmgr or curr component's spdid */
	*pgc  = cos_cap_cpy(resci, boot_info, CAP_PGTBL, a_ci->pgtbl_cap);
	assert(*pgc);
	*capc = cos_cap_cpy(resci, boot_info, CAP_CAPTBL, a_ci->captbl_cap);
	assert(*capc);
	*cc   = cos_cap_cpy(resci, boot_info, CAP_COMP, a_ci->comp_cap);
	assert(*cc);
	*psid = 0; /* TODO: find the parent */

	return (int)(resci->cap_frontier);
}

static int 
boot_comp_info_iter(capid_t curresfr, spdid_t *csid, pgtblcap_t *pgc, captblcap_t *capc, compcap_t *cc, spdid_t *psid)
{
	static int iter_idx = 1; /* skip llbooter component info! i'm guessing spdid == 0 is for booter */
	int ret = BOOT_CI_GET_ERROR;

	/* looks like the boot comps index start from 1 in that array */
	if (iter_idx > num_cobj) {
		*csid = 0;
		goto done;
	}

	*csid = iter_idx;
	ret   = boot_comp_info_get(curresfr, *csid, pgc, capc, cc, psid);
	if (ret == BOOT_CI_GET_ERROR) {
		*csid = 0;
		goto done;
	}
	iter_idx ++;

done:
	return ret;
}

static int
boot_comp_frontier_get(int spdid, vaddr_t *vasfr, capid_t *capfr)
{
	struct cos_compinfo *a_ci;

	/* looks like the boot comps index start from 1 in that array */
	if (spdid > num_cobj) {
		return BOOT_CI_GET_ERROR;
	}

	a_ci  = new_comp_cap_info[spdid].compinfo;
	assert(a_ci);

	*vasfr = a_ci->vas_frontier;
	*capfr = a_ci->cap_frontier;

	return 0;
}

static int
boot_comp_childschedspds_get(int spdid, u64_t *idbits)
{
	*idbits = new_comp_cap_info[spdid].childid_sched_bitf;

	return 0;
}

static int
boot_comp_childspds_get(int spdid, u64_t *idbits)
{
	*idbits = new_comp_cap_info[spdid].childid_bitf;

	return 0;
}

u32_t
llboot_entry(spdid_t curr, int op, u32_t arg3, u32_t arg4, u32_t *ret2, u32_t *ret3)
{
	u32_t ret1 = 0;
	u32_t error = (1 << 16) - 1;

	switch(op) {
	case LLBOOT_COMP_INIT_DONE:
	{
		boot_thd_done();
		break;
	}
	case LLBOOT_COMP_INFO_GET:
	{
		pgtblcap_t pgc;
		captblcap_t capc;
		compcap_t cc;
		spdid_t psid;
		int ret;

		/* only resource manager is allowed to use this function */
		assert(curr == resmgr_spdid);
		ret = boot_comp_info_get(arg4, arg3, &pgc, &capc, &cc, &psid);
		if (ret == BOOT_CI_GET_ERROR) { 
			ret1 = error;
			break;
		}

		ret1  = ret;
		*ret2 = (pgc << 16) | capc;
		*ret3 = (cc << 16) | psid;

		break;
	}
	case LLBOOT_COMP_INFO_NEXT:
	{
		pgtblcap_t pgc;
		captblcap_t capc;
		compcap_t cc;
		spdid_t csid, psid;
		int ret;

		/* only resource manager is allowed to use this function */
		assert(curr == resmgr_spdid);
		ret = boot_comp_info_iter(arg4, &csid, &pgc, &capc, &cc, &psid);
		if (ret == BOOT_CI_GET_ERROR) { 
			ret1 = error;
			break;
		}

		ret1  = (csid << 16) | ret;
		*ret2 = (pgc << 16) | capc;
		*ret3 = (cc << 16) | psid;

		break;
	}
	case LLBOOT_COMP_FRONTIER_GET:
	{
		vaddr_t vas;
		capid_t caps;
		int ret;

		/* only resource manager is allowed to use this function */
		assert(curr == resmgr_spdid);
		ret = boot_comp_frontier_get(arg3, &vas, &caps);
		if (ret) { 
			ret1 = error;
			break;
		}

		*ret2 = ((caps << 16) >> 16);
		*ret3 = vas;

		break;
	}
	case LLBOOT_COMP_INITTHD_GET:
	{
		capid_t capfr;

		/* only resource manager is allowed to use this function */
		assert(curr == resmgr_spdid);
		/* init-thread of components that booter created..*/
		thdcap_t t = boot_comp_initthd_get(arg3, &capfr);

		ret1 = t;
		*ret2 = capfr;

		break;
	}
	case LLBOOT_COMP_CHILDSPDIDS_GET:
	{
		u64_t idbits = 0;

		if (curr != resmgr_spdid) assert(curr == arg3);
		ret1 = boot_comp_childspds_get(arg3, &idbits);
		*ret2 = (u32_t)idbits;
		*ret3 = (u32_t)(idbits >> 32);

		break;
	}
	case LLBOOT_COMP_CHILDSCHEDSPDIDS_GET:
	{
		u64_t idbits = 0;

		if (curr != resmgr_spdid) assert(curr == arg3);
		ret1 = boot_comp_childschedspds_get(arg3, &idbits);
		*ret2 = (u32_t)idbits;
		*ret3 = (u32_t)(idbits >> 32);

		break;
	}
	default:
	{
		assert(0);

		ret1 = error;
	}
	}

	return ret1;
}
