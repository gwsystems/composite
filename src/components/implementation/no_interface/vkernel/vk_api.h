#ifndef VK_API_H
#define VK_API_H

#include "vk_types.h"

/* api */
void vk_vm_create(struct vms_info *vminfo, struct vkernel_info *vkinfo);
void vk_vm_io_init(struct vms_info *vminfo, struct vms_info *dom0info, struct vkernel_info *vkinfo);
void vk_vm_virtmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long start_ptr, unsigned long range);
void vk_vm_shmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz);
void vk_vm_shmem_map(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz);
void vk_vm_sched_init(struct vms_info *vminfo);

thdcap_t  dom0_vio_thdcap(unsigned int vmid);
tcap_t    dom0_vio_tcap(unsigned int vmid);
arcvcap_t dom0_vio_rcvcap(unsigned int vmid);
asndcap_t dom0_vio_asndcap(unsigned int vmid);

vaddr_t dom0_vio_shm_base(unsigned int vmid);

#endif /* VK_API_H */
