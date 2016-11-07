#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <cos_types.h>
#include <cos_kernel_api.h>

#define VM_COUNT        2	/* virtual machine count */
#define VM_UNTYPED_SIZE (1<<26) /* untyped memory per vm = 64MB */

#define VK_VM_SHM_BASE  0x80000000      /* shared memory region */
#define VM_SHM_SZ       (1<<20)	        /* Shared memory mapping for each vm = 4MB */
#define VM_SHM_ALL_SZ   ((VM_COUNT>0)?(VM_COUNT*VM_SHM_SZ):VM_SHM_SZ)

#define VM_BUDGET_FIXED 400000
#define VM_PRIO_FIXED   TCAP_PRIO_MAX

enum vm_captbl_layout {
	VM_CAPTBL_SELF_EXITTHD_BASE    = BOOT_CAPTBL_FREE,
	VM_CAPTBL_LAST_CAP             = VM_CAPTBL_SELF_EXITTHD_BASE + NUM_CPU_COS*CAP16B_IDSZ,
	VM_CAPTBL_FREE                 = round_up_to_pow2(VM_CAPTBL_LAST_CAP, CAPMAX_ENTRY_SZ),
};

struct vms_info {
	unsigned int id;
	struct cos_compinfo cinfo, shm_cinfo;

	thdcap_t initthd, exitthd;
	thdid_t inittid;
	tcap_t inittcap;
	arcvcap_t initrcv;
};

struct vkernel_info {
	struct cos_compinfo cinfo, shm_cinfo;

	thdcap_t termthd;
	asndcap_t vminitasnd[VM_COUNT];
};

#endif /* VK_TYPES_H */
