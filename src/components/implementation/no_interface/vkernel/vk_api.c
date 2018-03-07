#include "vk_api.h"

extern vaddr_t cos_upcall_entry;
/* extern functions */
extern void vm_init(void *);
extern void dom0_io_fn(void *);
extern void vm_io_fn(void *);

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

	cos_meminfo_init(&(vmcinfo->mi), BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE, vmutpt);
	ret = cos_defcompinfo_child_alloc(vmdci, (vaddr_t)&cos_upcall_entry, (vaddr_t)BOOT_MEM_VM_BASE,
					  VM_CAPTBL_FREE, 1);
	cos_compinfo_init(&(vminfo->shm_cinfo), vmcinfo->pgtbl_cap, vmcinfo->captbl_cap, vmcinfo->comp_cap,
			  (vaddr_t)VK_VM_SHM_BASE, VM_CAPTBL_FREE, vk_cinfo);

	printc("\tCreating and copying initial component capabilities\n");
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_CT, vk_cinfo, vmcinfo->captbl_cap);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_PT, vk_cinfo, vmcinfo->pgtbl_cap);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_UNTYPED_PT, vk_cinfo, vmutpt);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_COMP, vk_cinfo, vmcinfo->comp_cap);
	assert(ret == 0);

	printc("\tInit thread= cap:%x\n", (unsigned int)initaep->thd);

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

	ret = cos_cap_cpy_at(vmcinfo, VM_CAPTBL_SELF_SINV_BASE, vk_cinfo, vkinfo->sinv);
	assert(ret == 0);
}

void
vk_vm_sched_init(struct vms_info *vminfo)
{
	struct cos_compinfo *vk_cinfo      = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct cos_defcompinfo *vmdci      = &(vminfo->dci);
	struct cos_compinfo *vmcinfo       = cos_compinfo_get(vmdci);
	union sched_param_union spsameprio = {.c = {.type = SCHEDP_PRIO, .value = (vminfo->id + 1)}};
	union sched_param_union spsameC    = {.c = {.type = SCHEDP_BUDGET, .value = (VM_FIXED_BUDGET_MS * 1000)}};
	union sched_param_union spsameT    = {.c = {.type = SCHEDP_WINDOW, .value = (VM_FIXED_PERIOD_MS * 1000)}};
	int ret;

	vminfo->inithd = sl_thd_comp_init(vmdci, 1);
	assert(vminfo->inithd);

	sl_thd_param_set(vminfo->inithd, spsameprio.v);
	sl_thd_param_set(vminfo->inithd, spsameC.v);
	sl_thd_param_set(vminfo->inithd, spsameT.v);

	printc("\tsl_thd 0x%x created for thread = cap:%x, id=%u\n", (unsigned int)(vminfo->inithd),
	       (unsigned int)sl_thd_thdcap(vminfo->inithd), sl_thd_thdid(vminfo->inithd));
}

void
vk_vm_io_init(struct vms_info *vminfo, struct vms_info *dom0info, struct vkernel_info *vkinfo)
{
	struct cos_compinfo *vmcinfo = cos_compinfo_get(&vminfo->dci);
	struct cos_compinfo *d0cinfo = cos_compinfo_get(&dom0info->dci);
	struct cos_aep_info *d0aep   = vm_schedaep_get(dom0info);
	struct cos_aep_info *vmaep   = vm_schedaep_get(vminfo);
	struct cos_compinfo *vkcinfo = cos_compinfo_get(cos_defcompinfo_curr_get());
	struct dom0_io_info *d0io    = dom0info->dom0io;
	struct vm_io_info *  vio     = vminfo->vmio;
	int                  vmidx   = vminfo->id - 1;
	int                  ret;

	assert(vminfo && dom0info && vkinfo);
	assert(vminfo->id && !dom0info->id);
	assert(vmidx >= 0 && vmidx <= VM_COUNT - 1);

	d0io->iothds[vmidx] = cos_thd_alloc(vkcinfo, d0cinfo->comp_cap, dom0_io_fn, (void *)vminfo->id);
	assert(d0io->iothds[vmidx]);
	d0io->iotcaps[vmidx] = cos_tcap_alloc(vkcinfo);
	assert(d0io->iotcaps[vmidx]);
	d0io->iorcvs[vmidx] = cos_arcv_alloc(vkcinfo, d0io->iothds[vmidx], d0io->iotcaps[vmidx], vkcinfo->comp_cap,
					     d0aep->rcv);
	assert(d0io->iorcvs[vmidx]);
	ret = cos_cap_cpy_at(d0cinfo, dom0_vio_thdcap(vminfo->id), vkcinfo, d0io->iothds[vmidx]);
	assert(ret == 0);
	ret = cos_cap_cpy_at(d0cinfo, dom0_vio_tcap(vminfo->id), vkcinfo, d0io->iotcaps[vmidx]);
	assert(ret == 0);
	ret = cos_cap_cpy_at(d0cinfo, dom0_vio_rcvcap(vminfo->id), vkcinfo, d0io->iorcvs[vmidx]);
	assert(ret == 0);

	vio->iothd = cos_thd_alloc(vkcinfo, vmcinfo->comp_cap, vm_io_fn, (void *)vminfo->id);
	assert(vio->iothd);
	vio->iorcv = cos_arcv_alloc(vkcinfo, vio->iothd, vmaep->tc, vkcinfo->comp_cap, vmaep->rcv);
	assert(vio->iorcv);
	ret = cos_cap_cpy_at(vmcinfo, VM_CAPTBL_SELF_IOTHD_BASE, vkcinfo, vio->iothd);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, VM_CAPTBL_SELF_IORCV_BASE, vkcinfo, vio->iorcv);
	assert(ret == 0);

	d0io->ioasnds[vmidx] = cos_asnd_alloc(vkcinfo, vio->iorcv, vkcinfo->captbl_cap);
	assert(d0io->ioasnds[vmidx]);
	vio->ioasnd = cos_asnd_alloc(vkcinfo, d0io->iorcvs[vmidx], vkcinfo->captbl_cap);
	assert(vio->ioasnd);
	ret = cos_cap_cpy_at(d0cinfo, dom0_vio_asndcap(vminfo->id), vkcinfo, d0io->ioasnds[vmidx]);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, VM_CAPTBL_SELF_IOASND_BASE, vkcinfo, vio->ioasnd);
	assert(ret == 0);
}

void
vk_vm_virtmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long start_ptr, unsigned long range)
{
	vaddr_t src_pg, dst_pg;
	struct cos_compinfo *vmcinfo = cos_compinfo_get(&(vminfo->dci));
	struct cos_compinfo *vk_cinfo = cos_compinfo_get(cos_defcompinfo_curr_get());

	assert(vminfo && vkinfo);

	src_pg = (vaddr_t)cos_page_bump_allocn(vk_cinfo, range);
	assert(src_pg);

	memcpy((void *)src_pg, (void *)start_ptr, range);
	dst_pg = cos_mem_aliasn(vmcinfo, vk_cinfo, src_pg, range);
	assert(dst_pg);
}

void
vk_vm_shmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz)
{
	vaddr_t src_pg, dst_pg, addr;

	assert(vminfo && vminfo->id == 0 && vkinfo);
	assert(shm_ptr == round_up_to_pgd_page(shm_ptr));

	/* VM0: mapping in all available shared memory. */
	src_pg = (vaddr_t)cos_page_bump_allocn(&vkinfo->shm_cinfo, shm_sz);
	assert(src_pg);

	dst_pg = cos_mem_aliasn(&vminfo->shm_cinfo, &vkinfo->shm_cinfo, src_pg, shm_sz);
	assert(dst_pg);

	return;
}

void
vk_vm_shmem_map(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz)
{
	vaddr_t src_pg = (shm_sz * vminfo->id) + shm_ptr, dst_pg;

	assert(vminfo && vminfo->id && vkinfo);
	assert(shm_ptr == round_up_to_pgd_page(shm_ptr));

	dst_pg = cos_mem_aliasn(&vminfo->shm_cinfo, &vkinfo->shm_cinfo, src_pg, shm_sz);
	assert(dst_pg);

	return;
}

thdcap_t
dom0_vio_thdcap(unsigned int vmid)
{
	return DOM0_CAPTBL_SELF_IOTHD_SET_BASE + (CAP16B_IDSZ * (vmid - 1));
}

tcap_t
dom0_vio_tcap(unsigned int vmid)
{
	return DOM0_CAPTBL_SELF_IOTCAP_SET_BASE + (CAP16B_IDSZ * (vmid - 1));
}

arcvcap_t
dom0_vio_rcvcap(unsigned int vmid)
{
	return DOM0_CAPTBL_SELF_IORCV_SET_BASE + (CAP64B_IDSZ * (vmid - 1));
}

asndcap_t
dom0_vio_asndcap(unsigned int vmid)
{
	return DOM0_CAPTBL_SELF_IOASND_SET_BASE + (CAP64B_IDSZ * (vmid - 1));
}

vaddr_t
dom0_vio_shm_base(unsigned int vmid)
{
	return VK_VM_SHM_BASE + (VM_SHM_SZ * vmid);
}
