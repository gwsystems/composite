#ifndef VK_TYPES_H
#define VK_TYPES_H

#define VM_COUNT 2                /* total virtual machine count */
#define APP_START_ID 2
#define VM_UNTYPED_SIZE(vmid) (vmid == RUMP_SUB ? (1 << 27) : (1<<25))/* untyped memory per vm = 128MB */

#define VK_VM_SHM_BASE 0x80000000 /* shared memory region */
#define VM_SHM_SZ (1 << 20)       /* Shared memory mapping for each vm = 4MB */
#define VM_SHM_ALL_SZ ((VM_COUNT > 0) ? (VM_COUNT * VM_SHM_SZ) : VM_SHM_SZ)

#define APP_SUB_SHM_BASE 0x20000000
#define APP_SUB_SHM_SZ   (1<<22)

#define HPET_PERIOD_US (10 * 1000)

#define PARENT_PERIOD_US (1000)

#define VM_FIXED_PERIOD_MS 10
#define VM_FIXED_BUDGET_MS 3

#define VM_CAPTBL_FREE  BOOT_CAPTBL_FREE
#define RK_CAPTBL_FREE  BOOT_CAPTBL_FREE

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
	TIMER_SUB, /* SL_THD WITH ASND, VIO from LC with it's own budget (tcap_deleg), SINV for HPET INFO */
	UDP_APP, /* VIO to HA_HPET for HPET INFO, SL_THD in OWN COMP but NO ASND */
	DL_APP, /* SL_THD with TCAP shared between HA + HC, SINV to HA, ASYNC to LA */
};

enum vm_prio {
	RUMP_PRIO = 2,
	TIMER_PRIO = 1,
	UDP_PRIO = 2,
	DL_PRIO = 1,
};

enum rk_inv_ops {
	RK_INV_OP1 = 0,
	RK_INV_OP2,
	RK_GET_BOOT_DONE,
	RK_SOCKET,
	RK_BIND,
	RK_RECVFROM,
	RK_SENDTO,
	RK_LOGDATA,
};

enum timer_inv_ops {
	TIMER_APP_BLOCK = 0,
	TIMER_UPCOUNTER_WAIT,
	TIMER_GET_COUNTER,
};

#endif /* VK_TYPES_H */
