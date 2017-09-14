#ifndef VK_TYPES_H
#define VK_TYPES_H

#define VM_COUNT 4                /* virtual machine count */
#define APP_START_ID 2
#define VM_UNTYPED_SIZE(vmid) (vmid == RUMP_SUB ? (1 << 27) : (1<<25))/* untyped memory per vm = 128MB */

#define VK_VM_SHM_BASE 0x80000000 /* shared memory region */
#define VM_SHM_SZ (1 << 20)       /* Shared memory mapping for each vm = 4MB */
#define VM_SHM_ALL_SZ ((VM_COUNT > 0) ? (VM_COUNT * VM_SHM_SZ) : VM_SHM_SZ)

#define HPET_PERIOD_US (10 * 1000)

#define PARENT_PERIOD_US (5 * 1000)
#define CHILD_PERIOD_US  (1000)

#define VM_FIXED_PERIOD_MS 10
#define VM_FIXED_BUDGET_MS 5

#define VM_CAPTBL_SELF_VK_SINV_BASE  BOOT_CAPTBL_FREE
/* for now, one thread per app and one app per subsys */
#define VM_CAPTBL_SELF_APPTHD_BASE   round_up_to_pow2(VM_CAPTBL_SELF_VK_SINV_BASE + captbl_idsize(CAP_SINV), CAPMAX_ENTRY_SZ)
#define VM_CAPTBL_SELF_LAST_CAP      VM_CAPTBL_SELF_APPTHD_BASE + captbl_idsize(CAP_THD)
#define VM_CAPTBL_FREE               round_up_to_pow2(VM_CAPTBL_SELF_LAST_CAP, CAPMAX_ENTRY_SZ)

#define APP_CAPTBL_SELF_RK_SINV_BASE VM_CAPTBL_SELF_APPTHD_BASE 
#define APP_CAPTBL_SELF_TM_SINV_BASE round_up_to_pow2(APP_CAPTBL_SELF_RK_SINV_BASE + captbl_idsize(CAP_SINV), CAPMAX_ENTRY_SZ)
/* TODO: app async requests are 1 way! - polls shared memory for results and consumes when available */
#define APP_CAPTBL_SELF_IOSND_BASE   round_up_to_pow2(APP_CAPTBL_SELF_TM_SINV_BASE + captbl_idsize(CAP_SINV), CAPMAX_ENTRY_SZ) /* one cap! - each app async comms with only one subsys. */
#define APP_CAPTBL_SELF_LAST_CAP     APP_CAPTBL_SELF_IOSND_BASE + captbl_idsize(CAP_ASND)
#define APP_CAPTBL_FREE              round_up_to_pow2(APP_CAPTBL_SELF_LAST_CAP, CAPMAX_ENTRY_SZ)

/* complex system structure! - HA subsys has a server thread to serve LC app requests and LA subsys has a server thread to serve HC app requests.*/
#define SUB_CAPTBL_SELF_IOTHD_BASE   VM_CAPTBL_FREE 
#define SUB_CAPTBL_SELF_IORCV_BASE   round_up_to_pow2(SUB_CAPTBL_SELF_IOTHD_BASE + captbl_idsize(CAP_THD), CAPMAX_ENTRY_SZ)
#define SUB_CAPTBL_SELF_LAST_CAP     SUB_CAPTBL_SELF_IORCV_BASE + captbl_idsize(CAP_ARCV)
#define SUB_CAPTBL_FREE              round_up_to_pow2(SUB_CAPTBL_SELF_LAST_CAP, CAPMAX_ENTRY_SZ)

/* timer subsys server thread has it's own tcap which gets budget only from the LC app requests */
#define TM_CAPTBL_SELF_IOTCAP_BASE   SUB_CAPTBL_FREE 
#define TM_CAPTBL_SELF_LAST_CAP      TM_CAPTBL_SELF_IOTCAP_BASE + captbl_idsize(CAP_TCAP)
#define TM_CAPTBL_FREE               round_up_to_pow2(TM_CAPTBL_SELF_LAST_CAP, CAPMAX_ENTRY_SZ)

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

enum rk_inv_ops {
	RK_INV_OP1 = 0,
	RK_INV_OP2,
	RK_GET_BOOT_DONE,
};

enum timer_inv_ops {
	TIMER_APP_BLOCK = 0,
	TIMER_UPCOUNTER_WAIT,
	TIMER_GET_COUNTER,
};

#endif /* VK_TYPES_H */
