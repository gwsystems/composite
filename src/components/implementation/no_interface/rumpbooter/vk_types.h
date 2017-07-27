#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <cos_types.h>

#define VM_COUNT        2	/* virtual machine count */
#define VM_UNTYPED_SIZE (1<<27) /* untyped memory per vm = 128MB */

#define VK_VM_SHM_BASE  0x80000000      /* shared memory region */
#define VM_SHM_SZ       (1<<20)	        /* Shared memory mapping for each vm = 4MB */
#define VM_SHM_ALL_SZ   ((VM_COUNT>0)?(VM_COUNT*VM_SHM_SZ):VM_SHM_SZ)

#define VM_BUDGET_FIXED 400000
#define VM_PRIO_FIXED   TCAP_PRIO_MAX

enum vm_state {
	VM_RUNNING = 0,	
	VM_EXITED  = 1,
};

#endif /* VK_TYPES_H */
