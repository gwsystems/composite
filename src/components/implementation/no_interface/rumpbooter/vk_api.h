#ifndef VK_API_H
#define VK_API_H

#include "vk_types.h"

/* extern functions */
extern void vm_exit(void *);
extern void vm_init(void *);
extern void kernel_init(void *);
extern void dom0_io_fn(void *);
extern void vm_io_fn(void *);

/* api */
void vk_initcaps_init(struct vms_info *vminfo, struct vkernel_info *vkinfo);
void rk_initcaps_init(struct vms_info *vminfo, struct vkernel_info *vkinfo);
void vk_iocaps_init(struct vms_info *vminfo, struct vms_info *dom0info, struct vkernel_info *vkinfo);

void vk_virtmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long start_ptr, unsigned long range);
void vk_shmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz);
void vk_shmem_map(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz);

thdcap_t dom0_vio_thdcap(unsigned int spdid);
tcap_t dom0_vio_tcap(unsigned int spdid);
arcvcap_t dom0_vio_rcvcap(unsigned int spdid);
asndcap_t dom0_vio_asndcap(unsigned int spdid);

vaddr_t dom0_vio_shm_base(unsigned int spdid);

#endif /* VK_API_H */
