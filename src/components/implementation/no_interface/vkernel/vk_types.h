#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <cos_types.h>
#include <cos_kernel_api.h>

#define VM_COUNT 2                /* virtual machine count */
#define VM_UNTYPED_SIZE (1 << 26) /* untyped memory per vm = 64MB */

#define VK_VM_SHM_BASE 0x80000000 /* shared memory region */
#define VM_SHM_SZ (1 << 20)       /* Shared memory mapping for each vm = 4MB */
#define VM_SHM_ALL_SZ ((VM_COUNT > 0) ? (VM_COUNT * VM_SHM_SZ) : VM_SHM_SZ)

#define VM_BUDGET_FIXED 400000
#define VM_PRIO_FIXED TCAP_PRIO_MAX

enum vm_captbl_layout {
	VM_CAPTBL_SELF_EXITTHD_BASE = BOOT_CAPTBL_FREE,

	/* VM1~ I/O Capabilities layout */
	VM_CAPTBL_SELF_IOTHD_BASE =
	  round_up_to_pow2(VM_CAPTBL_SELF_EXITTHD_BASE + NUM_CPU_COS * CAP16B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_SELF_IORCV_BASE  = round_up_to_pow2(VM_CAPTBL_SELF_IOTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_SELF_IOASND_BASE = round_up_to_pow2(VM_CAPTBL_SELF_IORCV_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_SELF_LAST_CAP    = VM_CAPTBL_SELF_IOASND_BASE + CAP64B_IDSZ,
	VM_CAPTBL_FREE             = round_up_to_pow2(VM_CAPTBL_SELF_LAST_CAP, CAPMAX_ENTRY_SZ),
};

enum dom0_captbl_layout {
	/* DOM0 I/O Capabilities layout */
	DOM0_CAPTBL_SELF_IOTHD_SET_BASE = VM_CAPTBL_SELF_IOTHD_BASE,
	DOM0_CAPTBL_SELF_IOTCAP_SET_BASE =
	  round_up_to_pow2(DOM0_CAPTBL_SELF_IOTHD_SET_BASE + CAP16B_IDSZ * ((VM_COUNT > 1 ? VM_COUNT - 1 : 1)),
	                   CAPMAX_ENTRY_SZ),
	DOM0_CAPTBL_SELF_IORCV_SET_BASE =
	  round_up_to_pow2(DOM0_CAPTBL_SELF_IOTCAP_SET_BASE + CAP16B_IDSZ * ((VM_COUNT > 1 ? VM_COUNT - 1 : 1)),
	                   CAPMAX_ENTRY_SZ),
	DOM0_CAPTBL_SELF_IOASND_SET_BASE =
	  round_up_to_pow2(DOM0_CAPTBL_SELF_IORCV_SET_BASE + CAP64B_IDSZ * ((VM_COUNT > 1 ? VM_COUNT - 1 : 1)),
	                   CAPMAX_ENTRY_SZ),
	DOM0_CAPTBL_SELF_LAST_CAP =
	  DOM0_CAPTBL_SELF_IOASND_SET_BASE + CAP64B_IDSZ * ((VM_COUNT > 1 ? VM_COUNT - 1 : 1)),
	DOM0_CAPTBL_FREE = round_up_to_pow2(DOM0_CAPTBL_SELF_LAST_CAP, CAPMAX_ENTRY_SZ),
};

enum vm_state {
	VM_RUNNING = 0,
	VM_EXITED  = 1,
};

struct vm_io_info {
	thdcap_t iothd;
	arcvcap_t iorcv;
	asndcap_t ioasnd;
};

struct dom0_io_info {
	thdcap_t iothds[VM_COUNT - 1];
	tcap_t iotcaps[VM_COUNT - 1];
	arcvcap_t iorcvs[VM_COUNT - 1];
	asndcap_t ioasnds[VM_COUNT - 1];
};

struct vms_info {
	unsigned int id;
	struct cos_compinfo cinfo, shm_cinfo;

	unsigned int state;
	thdcap_t initthd, exitthd;
	thdid_t inittid;
	tcap_t inittcap;
	arcvcap_t initrcv;

	union { /* for clarity */
		struct vm_io_info *vmio;
		struct dom0_io_info *dom0io;
	};
};

struct vkernel_info {
	struct cos_compinfo cinfo, shm_cinfo;

	thdcap_t termthd;
	asndcap_t vminitasnd[VM_COUNT];
};

#endif /* VK_TYPES_H */
