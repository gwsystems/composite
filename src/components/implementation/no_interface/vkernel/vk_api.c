#include "vk_api.h"

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

