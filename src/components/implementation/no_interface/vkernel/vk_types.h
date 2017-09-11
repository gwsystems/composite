#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sl_thd.h>

#define VM_COUNT 2                /* virtual machine count */
#define VM_UNTYPED_SIZE (1 << 26) /* untyped memory per vm = 64MB */

#define VK_VM_SHM_BASE 0x80000000 /* shared memory region */
#define VM_SHM_SZ (1 << 20)       /* Shared memory mapping for each vm = 4MB */
#define VM_SHM_ALL_SZ ((VM_COUNT > 0) ? (VM_COUNT * VM_SHM_SZ) : VM_SHM_SZ)

#define VM_FIXED_PERIOD_MS 10
#define VM_FIXED_BUDGET_MS 5

#define VM_CAPTBL_SELF_SINV_BASE         BOOT_CAPTBL_FREE
/* VM1~ I/O Capabilities layout */
#define VM_CAPTBL_SELF_IOTHD_BASE        round_up_to_pow2(VM_CAPTBL_SELF_SINV_BASE + captbl_idsize(CAP_SINV), CAPMAX_ENTRY_SZ)
#define VM_CAPTBL_SELF_IORCV_BASE        round_up_to_pow2(VM_CAPTBL_SELF_IOTHD_BASE + captbl_idsize(CAP_THD), CAPMAX_ENTRY_SZ)
#define VM_CAPTBL_SELF_IOASND_BASE       round_up_to_pow2(VM_CAPTBL_SELF_IORCV_BASE + captbl_idsize(CAP_ARCV), CAPMAX_ENTRY_SZ)
#define VM_CAPTBL_SELF_LAST_CAP          VM_CAPTBL_SELF_IOASND_BASE + captbl_idsize(CAP_ASND)
#define VM_CAPTBL_FREE                   round_up_to_pow2(VM_CAPTBL_SELF_LAST_CAP, CAPMAX_ENTRY_SZ)

/* DOM0 I/O Capabilities layout */
#define DOM0_CAPTBL_SELF_IOTHD_SET_BASE  VM_CAPTBL_SELF_IOTHD_BASE
#define DOM0_CAPTBL_SELF_IOTCAP_SET_BASE round_up_to_pow2(DOM0_CAPTBL_SELF_IOTHD_SET_BASE + captbl_idsize(CAP_THD)*((VM_COUNT>1 ? VM_COUNT-1 : 1)), CAPMAX_ENTRY_SZ)
#define DOM0_CAPTBL_SELF_IORCV_SET_BASE  round_up_to_pow2(DOM0_CAPTBL_SELF_IOTCAP_SET_BASE + captbl_idsize(CAP_TCAP)*((VM_COUNT>1 ? VM_COUNT-1 : 1)), CAPMAX_ENTRY_SZ)
#define DOM0_CAPTBL_SELF_IOASND_SET_BASE round_up_to_pow2(DOM0_CAPTBL_SELF_IORCV_SET_BASE + captbl_idsize(CAP_ARCV)*((VM_COUNT>1 ? VM_COUNT-1 : 1)), CAPMAX_ENTRY_SZ)
#define DOM0_CAPTBL_SELF_LAST_CAP        DOM0_CAPTBL_SELF_IOASND_SET_BASE + captbl_idsize(CAP_ASND)*((VM_COUNT>1 ? VM_COUNT-1 : 1))
#define DOM0_CAPTBL_FREE                 round_up_to_pow2(DOM0_CAPTBL_SELF_LAST_CAP, CAPMAX_ENTRY_SZ)

enum vm_state
{
	VM_RUNNING = 0,
	VM_EXITED  = 1,
};

struct vm_io_info {
	thdcap_t  iothd;
	arcvcap_t iorcv;
	asndcap_t ioasnd;
};

struct dom0_io_info {
	thdcap_t  iothds[VM_COUNT - 1];
	tcap_t    iotcaps[VM_COUNT - 1];
	arcvcap_t iorcvs[VM_COUNT - 1];
	asndcap_t ioasnds[VM_COUNT - 1];
};

struct vms_info {
	unsigned int           id;
	struct cos_defcompinfo dci;
	struct cos_compinfo    shm_cinfo;
	struct sl_thd         *inithd;

	union { /* for clarity */
		struct vm_io_info *  vmio;
		struct dom0_io_info *dom0io;
	};
};

struct vkernel_info {
	struct cos_compinfo shm_cinfo;

	thdcap_t  termthd;
	sinvcap_t sinv;
	asndcap_t vminitasnd[VM_COUNT];
};

enum vkernel_server_option {
	VK_SERV_VM_ID = 0,
	VK_SERV_VM_EXIT,
};

extern struct vms_info vmx_info[];
extern struct dom0_io_info dom0ioinfo;
extern struct vm_io_info vmioinfo[];
extern struct vkernel_info vk_info;

#endif /* VK_TYPES_H */
