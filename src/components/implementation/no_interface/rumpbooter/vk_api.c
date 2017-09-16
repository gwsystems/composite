#include "vk_api.h"
#include "cos2rk_rb_api.h"

extern vaddr_t cos_upcall_entry;
extern int __inv_rk_inv_entry(int r1, int r2, int r3, int r4);
extern int __inv_timer_inv_entry(int r1, int r2, int r3, int r4);
/* extern functions */
extern void vm_init(void *);
extern void rump_io_fn(void *);
extern void timer_io_fn(void *);

static struct cos_aep_info *
vm_schedaep_get(struct vms_info *vminfo)
{ return cos_sched_aep_get(&(vminfo->dci)); }

void
vk_vm_create(struct vms_info *vminfo, struct vkernel_info *vkinfo)
{
	struct cos_compinfo    *vk_cinfo = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct cos_defcompinfo *vmdci    = &(vminfo->dci);
	struct cos_compinfo    *vmcinfo  = cos_compinfo_get(vmdci);
	struct cos_aep_info    *initaep  = cos_sched_aep_get(vmdci);
	pgtblcap_t              vmutpt;
	int                     ret;

	assert(vminfo && vkinfo);

	vmutpt = cos_pgtbl_alloc(vk_cinfo);
	assert(vmutpt);

	cos_meminfo_init(&(vmcinfo->mi), BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE(vminfo->id), vmutpt);

	ret = cos_defcompinfo_child_alloc(vmdci, (vaddr_t)&cos_upcall_entry, (vaddr_t)BOOT_MEM_VM_BASE,
					  VM_CAPTBL_FREE, vminfo->id < APP_START_ID ? 1 : 0);
	if (vminfo->id >= APP_START_ID) {
		int schidx = 0;
		struct cos_compinfo *schci = NULL;
		struct cos_aep_info *schaep = NULL;

		switch(vminfo->id) {
		case UDP_APP: schidx = RUMP_SUB; break;
		case DL_APP:  schidx = TIMER_SUB; break;
		default: assert(0);
		}

		schci  = cos_compinfo_get(&(vmx_info[schidx].dci));
		schaep = cos_sched_aep_get(&(vmx_info[schidx].dci));

		initaep->tc = schaep->tc;
		initaep->rcv = schaep->rcv;

		ret = cos_cap_cpy_at(schci, VM_CAPTBL_SELF_APPTHD_BASE, vk_cinfo, initaep->thd);
		assert(ret == 0);
	}

	cos_compinfo_init(&(vminfo->shm_cinfo), vmcinfo->pgtbl_cap, vmcinfo->captbl_cap, vmcinfo->comp_cap,
			  (vaddr_t)APP_SUB_SHM_BASE, VM_CAPTBL_FREE, vk_cinfo);

	printc("\tCreating and copying initial component capabilities\n");
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_CT, vk_cinfo, vmcinfo->captbl_cap);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_PT, vk_cinfo, vmcinfo->pgtbl_cap);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_UNTYPED_PT, vk_cinfo, vmutpt);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_COMP, vk_cinfo, vmcinfo->comp_cap);
	assert(ret == 0);

	printc("\tInit thread= cap:%x, thdid:%u\n", (unsigned int)initaep->thd, (thdid_t)cos_introspect(vk_cinfo, initaep->thd, THD_GET_TID));

	/*
	 * TODO: Multi-core support to create INITIAL Capabilities per core
	 */
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_INITTHD_BASE, vk_cinfo, initaep->thd);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_INITHW_BASE, vk_cinfo, BOOT_CAPTBL_SELF_INITHW_BASE);
	assert(ret == 0);

	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_INITTCAP_BASE, vk_cinfo, initaep->tc);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_INITRCV_BASE, vk_cinfo, initaep->rcv);
	assert(ret == 0);
}

void
vk_vm_sched_init(struct vms_info *vminfo)
{
	struct cos_compinfo *vk_cinfo  = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct cos_defcompinfo *vmdci  = &(vminfo->dci);
	struct cos_compinfo *vmcinfo   = cos_compinfo_get(vmdci);
	union sched_param_union spprio = {.c = {.type = SCHEDP_PRIO, .value = 0}};
	union sched_param_union spC    = {.c = {.type = SCHEDP_BUDGET, .value = (VM_FIXED_BUDGET_MS * 1000)}};
	union sched_param_union spT    = {.c = {.type = SCHEDP_WINDOW, .value = (VM_FIXED_PERIOD_MS * 1000)}};
	int ret, prio;

	vminfo->inithd = sl_thd_comp_init(vmdci, vminfo->id < APP_START_ID ? 1 : 0);
	assert(vminfo->inithd);

	if (vminfo->id >= APP_START_ID) return;

	switch (vminfo->id) {
	case TIMER_SUB: prio = PRIO_HIGH; break;
	case RUMP_SUB:  prio = PRIO_MID; break;
	default: assert(0);
	}
	spprio.c.value = prio;

	sl_thd_param_set(vminfo->inithd, spprio.v);
	sl_thd_param_set(vminfo->inithd, spC.v);
	sl_thd_param_set(vminfo->inithd, spT.v);

	printc("\tsl_thd 0x%x created for thread = cap:%x, id=%u\n", (unsigned int)(vminfo->inithd),
	       (unsigned int)sl_thd_thdcap(vminfo->inithd), (vminfo->inithd)->thdid);
}

void
vk_vm_virtmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long start_ptr, unsigned long range)
{
	vaddr_t src_pg;
	struct cos_compinfo *vmcinfo = cos_compinfo_get(&(vminfo->dci));
	struct cos_compinfo *vk_cinfo = cos_compinfo_get(cos_defcompinfo_curr_get());
	vaddr_t addr;

	assert(vminfo && vkinfo);


	src_pg = (vaddr_t)cos_page_bump_allocn(vk_cinfo, range);
	assert(src_pg);

	for (addr = 0; addr < range; addr += PAGE_SIZE, src_pg += PAGE_SIZE) {
		vaddr_t dst_pg;

		memcpy((void *)src_pg, (void *)(start_ptr + addr), PAGE_SIZE);

		dst_pg = cos_mem_alias(vmcinfo, vk_cinfo, src_pg);
		assert(dst_pg);
	}
}

void
vk_vm_shmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz)
{
	int i;
	vaddr_t src_pg, dst_pg, addr;

	assert(vminfo && vkinfo);
	assert(shm_ptr == round_up_to_pgd_page(shm_ptr));

	/* VM0: mapping in all available shared memory. */
	src_pg = (vaddr_t)cos_page_bump_allocn(&vkinfo->shm_cinfo, shm_sz);
	assert(src_pg);

	for (addr = shm_ptr; addr < (shm_ptr + shm_sz); addr += PAGE_SIZE, src_pg += PAGE_SIZE) {
		//assert(src_pg == addr);

		dst_pg = cos_mem_alias(&vminfo->shm_cinfo, &vkinfo->shm_cinfo, src_pg);
		assert(dst_pg && dst_pg == addr);
	}

	/* cos2rk ring buffer creation */
	//cos2rk_rb_init();

	return;
}

void
vk_vm_shmem_map(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz)
{
	vaddr_t src_pg, dst_pg, addr;

	assert(vminfo && vminfo->id && vkinfo);
	assert(shm_ptr == round_up_to_pgd_page(shm_ptr));

	if (vminfo->id == UDP_APP) {
		assert(TIMER_SUB == 1);
		src_pg = shm_ptr + shm_sz;
	} else if (vminfo->id == DL_APP) {
		assert(RUMP_SUB == 0);
		src_pg = shm_ptr;
	} else {
		assert(0);
	}

	for (addr = shm_ptr; addr < (shm_ptr + shm_sz); addr += PAGE_SIZE, src_pg += PAGE_SIZE) {
		/* VMx: mapping in only a section of shared-memory to share with VM0 */
		assert(src_pg);

		dst_pg = cos_mem_alias(&vminfo->shm_cinfo, &vkinfo->shm_cinfo, src_pg);
		assert(dst_pg && dst_pg == addr);
	}

	return;
}

void
vk_vm_sinvs_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo)
{
	struct cos_compinfo *vk_cinfo = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct cos_compinfo *vm_cinfo = cos_compinfo_get(&vminfo->dci);
	int ret;

	ret = cos_cap_cpy_at(vm_cinfo, VM_CAPTBL_SELF_VK_SINV_BASE, vk_cinfo, vkinfo->sinv);
	assert(ret == 0);

	switch(vminfo->id) {
	case RUMP_SUB:
	{
		vminfo->sinv = cos_sinv_alloc(vk_cinfo, vm_cinfo->comp_cap, (vaddr_t)__inv_rk_inv_entry);
		assert(vminfo->sinv);

		break;
	}
	case TIMER_SUB:
	{
		vminfo->sinv = cos_sinv_alloc(vk_cinfo, vm_cinfo->comp_cap, (vaddr_t)__inv_timer_inv_entry);
		assert(vminfo->sinv);

		break;
	}
	case UDP_APP:
	case DL_APP:
	{
		struct vms_info *rk_info = &vmx_info[RUMP_SUB];
		struct vms_info *tm_info = &vmx_info[TIMER_SUB];
		sinvcap_t rk_inv, tm_inv;

		printc("\tSetting up sinv capability from user component to kernel component\n");

		ret = cos_cap_cpy_at(vm_cinfo, APP_CAPTBL_SELF_RK_SINV_BASE, vk_cinfo, rk_info->sinv);
		assert(ret == 0);

		ret = cos_cap_cpy_at(vm_cinfo, APP_CAPTBL_SELF_TM_SINV_BASE, vk_cinfo, tm_info->sinv);
		assert(ret == 0);

		break;
	}
	default: assert(0);
	}
}

void
vk_iocomm_init(void)
{
	struct cos_compinfo *vkcinfo = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct vms_info *rk, *tm, *udp, *dl;
	struct cos_compinfo *rkci, *tmci, *udpci, *dlci;
	int ret;

	assert(VM_COUNT == 4);
	rk  = &vmx_info[RUMP_SUB];
	tm  = &vmx_info[TIMER_SUB];
	udp = &vmx_info[UDP_APP];
	dl  = &vmx_info[DL_APP];

	rkci  = cos_compinfo_get(&rk->dci);
	tmci  = cos_compinfo_get(&tm->dci);
	udpci = cos_compinfo_get(&udp->dci);
	dlci  = cos_compinfo_get(&dl->dci);

	printc("Creating required I/O capabilities - based on complex communication policies\n");

	/* I/O in RUMP_SUB */
	rk->iothd = cos_thd_alloc(vkcinfo, rkci->comp_cap, rump_io_fn, NULL);
	assert(rk->iothd);
	rk->iorcv = cos_arcv_alloc(vkcinfo, rk->iothd, sl_thd_tcap(rk->inithd), vkcinfo->comp_cap, sl_thd_rcvcap(rk->inithd));
	assert(rk->iorcv);
	ret = cos_cap_cpy_at(rkci, SUB_CAPTBL_SELF_IOTHD_BASE, vkcinfo, rk->iothd);
	assert(ret == 0);
	ret = cos_cap_cpy_at(rkci, SUB_CAPTBL_SELF_IORCV_BASE, vkcinfo, rk->iorcv);
	assert(ret == 0);

	rk->ioasnd = cos_asnd_alloc(vkcinfo, rk->iorcv, vkcinfo->captbl_cap);
	assert(rk->ioasnd);
	/* copy asnd to rk into dl */
	ret = cos_cap_cpy_at(dlci, APP_CAPTBL_SELF_IOSND_BASE, vkcinfo, rk->ioasnd);
	assert(ret == 0);

	/* I/O in TIMER_SUB */
	tm->iothd = cos_thd_alloc(vkcinfo, tmci->comp_cap, timer_io_fn, NULL);
	assert(tm->iothd);
	tm->iotcap = cos_tcap_alloc(vkcinfo);
	assert(tm->iotcap);
	tm->iorcv = cos_arcv_alloc(vkcinfo, tm->iothd, tm->iotcap, vkcinfo->comp_cap, sl_thd_rcvcap(tm->inithd));
	assert(tm->iorcv);
	ret = cos_cap_cpy_at(tmci, SUB_CAPTBL_SELF_IOTHD_BASE, vkcinfo, tm->iothd);
	assert(ret == 0);
	ret = cos_cap_cpy_at(tmci, SUB_CAPTBL_SELF_IORCV_BASE, vkcinfo, tm->iorcv);
	assert(ret == 0);
	ret = cos_cap_cpy_at(tmci, TM_CAPTBL_SELF_IOTCAP_BASE, vkcinfo, tm->iotcap);
	assert(ret == 0);

	tm->ioasnd = cos_asnd_alloc(vkcinfo, tm->iorcv, vkcinfo->captbl_cap);
	assert(tm->ioasnd);
	/* copy asnd to timer into udp */
	ret = cos_cap_cpy_at(udpci, APP_CAPTBL_SELF_IOSND_BASE, vkcinfo, tm->ioasnd);
	assert(ret == 0);
}

vaddr_t
dom0_vio_shm_base(unsigned int vmid)
{
	return VK_VM_SHM_BASE + (VM_SHM_SZ * vmid);
}
