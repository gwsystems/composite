#ifndef VK_TYPES_H
#define VK_TYPES_H

#define VM_COUNT 2                /* virtual machine count */
#define VM_UNTYPED_SIZE (1 << 27) /* untyped memory per vm = 128MB */
#define USERSPACE_VM 1
#define KERNEL_VM 0

#define VK_VM_SHM_BASE 0x80000000 /* shared memory region */
#define VM_SHM_SZ (1 << 20)       /* Shared memory mapping for each vm = 4MB */
#define VM_SHM_ALL_SZ ((VM_COUNT > 0) ? (VM_COUNT * VM_SHM_SZ) : VM_SHM_SZ)

#define HPET_PERIOD_US (10 * 1000)

#define VM_FIXED_PERIOD_MS 10
#define VM_FIXED_BUDGET_MS 5

#define VM_CAPTBL_SELF_VK_SINV_BASE      BOOT_CAPTBL_FREE
#define VM_CAPTBL_SELF_RK_SINV_BASE      round_up_to_pow2(VM_CAPTBL_SELF_VK_SINV_BASE + captbl_idsize(CAP_SINV), CAPMAX_ENTRY_SZ)
/* INV to TIMER SUBSYSTEM */
#define VM_CAPTBL_SELF_TM_SINV_BASE      round_up_to_pow2(VM_CAPTBL_SELF_RK_SINV_BASE + captbl_idsize(CAP_SINV), CAPMAX_ENTRY_SZ)
/* VM1~ I/O Capabilities layout */
#define VM_CAPTBL_SELF_IOTHD_BASE        round_up_to_pow2(VM_CAPTBL_SELF_TM_SINV_BASE + captbl_idsize(CAP_SINV), CAPMAX_ENTRY_SZ)
#define VM_CAPTBL_SELF_IORCV_BASE        round_up_to_pow2(VM_CAPTBL_SELF_IOTHD_BASE + captbl_idsize(CAP_THD), CAPMAX_ENTRY_SZ)
#define VM_CAPTBL_SELF_IOASND_BASE       round_up_to_pow2(VM_CAPTBL_SELF_IORCV_BASE + captbl_idsize(CAP_ARCV), CAPMAX_ENTRY_SZ)
#define VM_CAPTBL_SELF_LAST_CAP          VM_CAPTBL_SELF_IOASND_BASE + captbl_idsize(CAP_ASND)
#define VM_CAPTBL_FREE                   round_up_to_pow2(VM_CAPTBL_SELF_LAST_CAP, CAPMAX_ENTRY_SZ)

/* DOM0 I/O Capabilities layout */
#define DOM0_CAPTBL_SELF_IOTHD_SET_BASE  VM_CAPTBL_SELF_IOTHD_BASE
#define DOM0_CAPTBL_SELF_IORCV_SET_BASE  round_up_to_pow2(DOM0_CAPTBL_SELF_IOTHD_SET_BASE + captbl_idsize(CAP_THD)*((VM_COUNT>1 ? VM_COUNT-1 : 1)), CAPMAX_ENTRY_SZ)
#define DOM0_CAPTBL_SELF_IOASND_SET_BASE round_up_to_pow2(DOM0_CAPTBL_SELF_IORCV_SET_BASE + captbl_idsize(CAP_ARCV)*((VM_COUNT>1 ? VM_COUNT-1 : 1)), CAPMAX_ENTRY_SZ)
#define DOM0_CAPTBL_SELF_LAST_CAP        DOM0_CAPTBL_SELF_IOASND_SET_BASE + captbl_idsize(CAP_ASND)*((VM_COUNT>1 ? VM_COUNT-1 : 1))
#define DOM0_CAPTBL_FREE                 round_up_to_pow2(DOM0_CAPTBL_SELF_LAST_CAP, CAPMAX_ENTRY_SZ)

enum vkernel_server_option {
        VK_SERV_VM_ID = 0,
	VK_SERV_VM_BLOCK,
        VK_SERV_VM_EXIT,
	VK_SERV_SHM_VADDR_GET,
	VK_SERV_SHM_ALLOC,
	VK_SERV_SHM_DEALLOC,
	VK_SERV_SHM_MAP,
};

enum vm_types {
	RUMP_SUB = 0, /* SL THD WITH ASND, VIO from HC_DL for logging with LA budget, SINV for POSIX API */
	UDP_APP, /* VIO to HA_HPET for HPET INFO, SL_THD in OWN COMP but NO ASND */
	TIMER_SUB, /* SL_THD WITH ASND, VIO from LC with it's own budget (tcap_deleg), SINV for HPET INFO */
	DL_APP, /* SL_THD with TCAP shared between HA + HC, SINV to HA, ASYNC to LA */
};

enum rk_inv_ops {
	RK_INV_OP1 = 0,
	RK_INV_OP2,
	RK_GET_BOOT_DONE,
};

#endif /* VK_TYPES_H */
