#include "vk_api.h"
#include "cos2rk_rb_api.h"

extern vaddr_t cos_upcall_entry;

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
	assert(vminfo->id == RUMP_SUB);

	vmutpt = cos_pgtbl_alloc(vk_cinfo);
	assert(vmutpt);

	cos_meminfo_init(&(vmcinfo->mi), BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE(vminfo->id), vmutpt);

	ret = cos_defcompinfo_child_alloc(vmdci, (vaddr_t)&cos_upcall_entry, (vaddr_t)BOOT_MEM_VM_BASE,
				  VM_CAPTBL_FREE, vminfo->id < APP_START_ID ? 1 : 0);
	assert(ret == 0);

	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_INITTCAP_BASE, vk_cinfo, initaep->tc);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_INITRCV_BASE, vk_cinfo, initaep->rcv);
	assert(ret == 0);

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
}

void
vk_vm_sched_init(struct vms_info *vminfo)
{
        struct cos_defcompinfo *vmdci  = &(vminfo->dci);
        struct cos_compinfo *vmcinfo   = cos_compinfo_get(vmdci);

	assert(vminfo->id == RUMP_SUB);
        vminfo->inithd = sl_thd_comp_init(vmdci, vminfo->id < APP_START_ID ? 1 : 0);
        assert(vminfo->inithd);

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
	assert(vminfo->id == RUMP_SUB);

	src_pg = (vaddr_t)cos_page_bump_allocn(vk_cinfo, range);
	assert(src_pg);
	printc("\tVM start:0x%x\n", (unsigned int)src_pg);

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
	assert(0);
	return;
}

void
vk_vm_shmem_map(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz)
{
	assert(0);
	return;
}

void
vk_vm_sinvs_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo)
{
	assert(0);
	return;
}

void
vk_iocomm_init(void)
{
	assert(0);
	return;
}

vaddr_t
dom0_vio_shm_base(unsigned int vmid)
{
	assert(0);
	return VK_VM_SHM_BASE + (VM_SHM_SZ * vmid);
}
