#ifndef VK_API_H
#define VK_API_H

#include "vk_types.h"

/* extern functions */
extern void vm_init(void *);
extern void dom0_io_fn(void *);
extern void vm_io_fn(void *);
extern void *__inv_vkernel_serverfn(int a, int b, int c);

extern void scheduler(void);

/* api */
void vk_initcaps_init(struct vms_info *vminfo, struct vkernel_info *vkinfo);
void vk_iocaps_init(struct vms_info *vminfo, struct vms_info *dom0info, struct vkernel_info *vkinfo);

void vk_virtmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long start_ptr, unsigned long range);
void vk_shmem_alloc(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz);
void vk_shmem_map(struct vms_info *vminfo, struct vkernel_info *vkinfo, unsigned long shm_ptr, unsigned long shm_sz);

void vk_sl_thd_init(struct vms_info *vminfo);

thdcap_t dom0_vio_thdcap(unsigned int vmid);
tcap_t dom0_vio_tcap(unsigned int vmid);
arcvcap_t dom0_vio_rcvcap(unsigned int vmid);
asndcap_t dom0_vio_asndcap(unsigned int vmid);

vaddr_t dom0_vio_shm_base(unsigned int vmid);

#endif /* VK_API_H */
