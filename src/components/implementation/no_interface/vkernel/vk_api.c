#include "vk_api.h"

void
vk_initcaps_init(struct vms_info *vminfo, struct vkernel_info *vkinfo)
{
	struct cos_compinfo *vmcinfo = &vminfo->cinfo;
	struct cos_compinfo *vkcinfo = &vkinfo->cinfo;
	int ret;

	assert(vminfo && vkinfo);	

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
vk_iocaps_init(struct vms_info *vminfo, struct vms_info *dom0info, struct vkernel_info *vkinfo)
{
	struct cos_compinfo *vmcinfo = &vminfo->cinfo;
	struct cos_compinfo *d0cinfo = &dom0info->cinfo;
	struct cos_compinfo *vkcinfo = &vkinfo->cinfo;
	struct dom0_io_info *d0io    = dom0info->dom0io;
	struct vm_io_info   *vio     = vminfo->vmio;
	int vmidx                    = vminfo->id - 1;
	int ret;

	assert(vminfo && dom0info && vkinfo);	
	assert(vminfo->id && !dom0info->id);
	assert(vmidx >= 0 && vmidx <= VM_COUNT - 1);

	d0io->iothds[vmidx] = cos_thd_alloc(vkcinfo, d0cinfo->comp_cap, dom0_io_fn, (void *)vminfo->id);
	assert(d0io->iothds[vmidx]);
	d0io->iotcaps[vmidx] = cos_tcap_alloc(vkcinfo, VM_PRIO_FIXED);
	assert(d0io->iotcaps[vmidx]);
	d0io->iorcvs[vmidx] = cos_arcv_alloc(vkcinfo, d0io->iothds[vmidx], d0io->iotcaps[vmidx], vkcinfo->comp_cap, dom0info->initrcv);
	assert(d0io->iorcvs[vmidx]);
	ret = cos_cap_cpy_at(d0cinfo, dom0_vio_thdcap(vminfo->id), vkcinfo, d0io->iothds[vmidx]);
	assert(ret == 0);
	ret = cos_cap_cpy_at(d0cinfo, dom0_vio_tcap(vminfo->id), vkcinfo, d0io->iotcaps[vmidx]);
	assert(ret == 0);
	ret = cos_cap_cpy_at(d0cinfo, dom0_vio_rcvcap(vminfo->id), vkcinfo, d0io->iorcvs[vmidx]);
	assert(ret == 0);

	vio->iothd = cos_thd_alloc(vkcinfo, vmcinfo->comp_cap, vm_io_fn, (void *)vminfo->id);
	assert(vio->iothd);
	vio->iorcv = cos_arcv_alloc(vkcinfo, vio->iothd, vminfo->inittcap, vkcinfo->comp_cap, vminfo->initrcv);
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
vk_virtmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo,
		 unsigned long start_ptr, unsigned long range)
{
	vaddr_t addr;

	assert(vminfo && vkinfo);	

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

	assert(vminfo && vminfo->id == 0 && vkinfo);
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

	assert(vminfo && vminfo->id && vkinfo);
	assert(shm_ptr == round_up_to_pgd_page(shm_ptr));

	for (addr = shm_ptr ; addr < (shm_ptr + shm_sz) ; addr += PAGE_SIZE, src_pg += PAGE_SIZE) {
		/* VMx: mapping in only a section of shared-memory to share with VM0 */
		assert(src_pg);

		dst_pg = cos_mem_alias(&vminfo->shm_cinfo, &vkinfo->shm_cinfo, src_pg);
		assert(dst_pg && dst_pg == addr);
	}	

	return;
}

thdcap_t
dom0_vio_thdcap(unsigned int vmid)
{ return DOM0_CAPTBL_SELF_IOTHD_SET_BASE + (CAP16B_IDSZ * (vmid-1)); }

tcap_t
dom0_vio_tcap(unsigned int vmid)
{ return DOM0_CAPTBL_SELF_IOTCAP_SET_BASE + (CAP16B_IDSZ * (vmid-1)); }

arcvcap_t
dom0_vio_rcvcap(unsigned int vmid)
{ return DOM0_CAPTBL_SELF_IORCV_SET_BASE + (CAP64B_IDSZ * (vmid-1)); }

asndcap_t
dom0_vio_asndcap(unsigned int vmid)
{ return DOM0_CAPTBL_SELF_IOASND_SET_BASE + (CAP64B_IDSZ * (vmid-1)); }
