#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <cos_types.h>

enum vm_captbl_layout {
	VM_CAPTBL_SELF_EXITTHD_BASE    = BOOT_CAPTBL_FREE,
	VM_CAPTBL_LAST_CAP             = VM_CAPTBL_SELF_EXITTHD_BASE + NUM_CPU_COS*CAP16B_IDSZ,
	VM_CAPTBL_FREE                 = round_up_to_pow2(VM_CAPTBL_LAST_CAP, CAPMAX_ENTRY_SZ),
};

#define VM_COUNT        2	/* virtual machine count */
#define VM_UNTYPED_SIZE (1<<26) /* untyped memory per vm = 64MB */

#endif /* VK_TYPES_H */
