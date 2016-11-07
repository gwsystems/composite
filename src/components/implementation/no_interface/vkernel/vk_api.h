#ifndef VK_API_H
#define VK_API_H

#include "vk_types.h"

void vk_shmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz);
void vk_shmem_map(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz);

#endif /* VK_API_H */
