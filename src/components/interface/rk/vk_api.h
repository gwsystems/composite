#ifndef VK_API_H
#define VK_API_H

#include <sinv_calls.h>

#include "vk_types.h"
#include "vk_structs.h"

/* api */
void vk_vm_create(struct vms_info *vminfo, struct vkernel_info *vkinfo);
void vk_vm_virtmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long start_ptr, unsigned long range);
void vk_vm_shmem_alloc();
void vk_vm_shmem_map();
void vk_vm_sched_init(struct vms_info *vminfo);
void vk_vm_sinvs_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo);
void vk_iocomm_init(void);

vaddr_t dom0_vio_shm_base(unsigned int vmid);

int vk_vm_id(void);
void vk_vm_exit(void);
void vk_vm_block(tcap_time_t timeout);

#endif /* VK_API_H */
