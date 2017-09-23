#ifndef COS2RK_TYPES_H
#define COS2RK_TYPES_H

#include "vk_types.h"

#define HW_ISR_LINES 32
#define HW_ISR_FIRST 1

/* The amount of memory to give RK to start in bytes */
#define RK_TOTAL_MEM (1<<26) //64MB

#define RK_IRQ_IO 15

#define COS2RK_MEM_SHM_BASE	VK_VM_SHM_BASE
#define COS2RK_VIRT_MACH_COUNT	VM_COUNT
#define COS2RK_VIRT_MACH_MEM_SZ(vmid) VM_UNTYPED_SIZE(vmid) //128MB
#define COS2RK_SHM_VM_SZ	VM_SHM_SZ //4MB
#define COS2RK_SHM_ALL_SZ	VM_SHM_ALL_SZ

extern unsigned int cycs_per_usec;
extern u64_t t_vm_cycs;
extern u64_t t_dom_cycs;

#endif /* COS2RK_TYPES_H */
