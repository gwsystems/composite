#include "vk_api.h"

void
vk_initcaps_init(struct vms_info *vminfo, struct vkernel_info *vkinfo)
{
	struct cos_compinfo *vmcinfo = &vminfo->cinfo;
	struct cos_compinfo *vkcinfo = &vkinfo->cinfo;
	int ret;

	vminfo->exitthd = cos_thd_alloc(vkcinfo, vkcinfo->comp_cap, vm_exit, (void *)vminfo->id);
	assert(vminfo->exitthd);


	vminfo->initthd = cos_thd_alloc(vkcinfo, vmcinfo->comp_cap, vm_init, (void *)vminfo->id);
	assert(vminfo->initthd);
	vminfo->inittid = (thdid_t)cos_introspect(vkcinfo, vminfo->initthd, THD_GET_TID);
	printc("\tInit thread= cap:%x tid:%x\n", (unsigned int)vminfo->initthd, (unsigned int)vminfo->inittid);

	printc("\tCreating and copying required initial capabilities\n");
	/*
	 * TODO: Multi-core support to create INITIAL Capabilities per core
	 */
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_INITTHD_BASE, vkcinfo, vminfo->initthd);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_INITHW_BASE, vkcinfo, BOOT_CAPTBL_SELF_INITHW_BASE);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, VM_CAPTBL_SELF_EXITTHD_BASE, vkcinfo, vminfo->exitthd);
	assert(ret == 0);

	vminfo->inittcap = cos_tcap_alloc(vkcinfo, TCAP_PRIO_MAX);
	assert(vminfo->inittcap);

	vminfo->initrcv = cos_arcv_alloc(vkcinfo, vminfo->initthd, vminfo->inittcap, vkcinfo->comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
	assert(vminfo->initrcv);

	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_INITTCAP_BASE, vkcinfo, vminfo->inittcap);
	assert(ret == 0);
	ret = cos_cap_cpy_at(vmcinfo, BOOT_CAPTBL_SELF_INITRCV_BASE, vkcinfo, vminfo->initrcv);
	assert(ret == 0);

	/*
	 * Create send end-point in VKernel to each VM's INITRCV end-point
	 */
	vkinfo->vminitasnd[vminfo->id] = cos_asnd_alloc(vkcinfo, vminfo->initrcv, vkcinfo->captbl_cap);
	assert(vkinfo->vminitasnd[vminfo->id]);
}

void
vk_virtmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo,
		 unsigned long start_ptr, unsigned long range)
{
	vaddr_t addr;

	for (addr = 0 ; addr < range ; addr += PAGE_SIZE) {
		vaddr_t src_pg = (vaddr_t)cos_page_bump_alloc(&vkinfo->cinfo), dst_pg;
		assert(src_pg);

		memcpy((void *)src_pg, (void *)(start_ptr + addr), PAGE_SIZE);

		dst_pg = cos_mem_alias(&vminfo->cinfo, &vkinfo->cinfo, src_pg);
		assert(dst_pg);
	}
}

void
vk_shmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, 
	       unsigned long shm_ptr, unsigned long shm_sz)
{
	vaddr_t src_pg = (shm_sz * vminfo->id) + shm_ptr, dst_pg, addr;

	assert(vminfo && vminfo->id == 0);
	assert(shm_ptr == round_up_to_pgd_page(shm_ptr));

	for (addr = shm_ptr ; addr < (shm_ptr + shm_sz) ; addr += PAGE_SIZE, src_pg += PAGE_SIZE) {
		/* VM0: mapping in all available shared memory. */
		src_pg = (vaddr_t)cos_page_bump_alloc(&vkinfo->shm_cinfo);
		assert(src_pg && src_pg == addr);

		dst_pg = cos_mem_alias(&vminfo->shm_cinfo, &vkinfo->shm_cinfo, src_pg);
		assert(dst_pg && dst_pg == addr);
	}	

	return;
}

void
vk_shmem_map(struct vms_info *vminfo, struct vkernel_info *vkinfo, 
	     unsigned long shm_ptr, unsigned long shm_sz)
{
	vaddr_t src_pg = (shm_sz * vminfo->id) + shm_ptr, dst_pg, addr;

	assert(vminfo && vminfo->id);
	assert(shm_ptr == round_up_to_pgd_page(shm_ptr));

	for (addr = shm_ptr ; addr < (shm_ptr + shm_sz) ; addr += PAGE_SIZE, src_pg += PAGE_SIZE) {
		/* VMx: mapping in only a section of shared-memory to share with VM0 */
		assert(src_pg);

		dst_pg = cos_mem_alias(&vminfo->shm_cinfo, &vkinfo->shm_cinfo, src_pg);
		assert(dst_pg && dst_pg == addr);
	}	

	return;
}

