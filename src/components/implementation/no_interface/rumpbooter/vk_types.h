#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <cos_types.h>

#define COS_VIRT_MACH_COUNT 4
#define COS_VIRT_MACH_MEM_SZ (1<<26) //64MB

#define COS_SHM_VM_SZ (1<<20) //4MB
#define COS_SHM_ALL_SZ (((COS_VIRT_MACH_COUNT - 1) > 0 ? (COS_VIRT_MACH_COUNT - 1) : 1) * COS_SHM_VM_SZ) //shared regions with VM 0

#define VM_TIMESLICE (1000*10) //10*1000*cycs_per_usec = 10ms
#define VM_MIN_TIMESLICE (1) //1us

#undef __SIMPLE_XEN_LIKE_TCAPS__
#define __SIMPLE_DISTRIBUTED_TCAPS__
#undef __INTELLIGENT_TCAPS__

#define HW_ISR_LINES 32
#define HW_ISR_FIRST 1

capid_t irq_thdcap[HW_ISR_LINES]; 
thdid_t irq_thdid[HW_ISR_LINES];
tcap_t irq_tcap[HW_ISR_LINES]; 
capid_t irq_arcvcap[HW_ISR_LINES];
tcap_prio_t irq_prio[HW_ISR_LINES];

enum vm_prio {
#if defined(__INTELLIGENT_TCAPS__)
#elif defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	PRIO_HIGH  = TCAP_PRIO_MAX,
	PRIO_LOW   = TCAP_PRIO_MAX + 100,
	PRIO_MID   = TCAP_PRIO_MAX + 50,
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	PRIO_BOOST = TCAP_PRIO_MAX,
	PRIO_OVER  = TCAP_PRIO_MAX + 100,
	PRIO_UNDER = TCAP_PRIO_MAX + 50,
#endif
};


#if defined(__SIMPLE_DISTRIBUTED_TCAPS__)
#define RIO_PRIO    PRIO_MID /* REAL I/O Priority */
#define VIO_PRIO    PRIO_HIGH /* Virtual I/O Priority */
#define RK_THD_PRIO PRIO_LOW /* Rumpkernel thread priority */

#define IO_BOUND_VM  1  /* VM that is I/O Bound */
#define CPU_BOUND_VM (COS_VIRT_MACH_COUNT  + 1) /* VM that is CPU Bound: Set to a val beyond number of VMs, so no CPU BOUND VM */ 

#define VIO_BUDGET_MAX (VM_TIMESLICE) /* Maximum a I/O Tcap in DOM0 should have */
#define VIO_BUDGET_THR (VM_MIN_TIMESLICE * 1000) /* minimum excess budget with I/O tcap to transfer/delegate back to VM */

#define VIO_BUDGET_APPROX (VM_MIN_TIMESLICE * 1000) /* Hack: budget transfered from I/O Tcap to DOM0's Tcap */
#endif

enum vm_status {
	VM_RUNNING,
	VM_BLOCKED,
	VM_EXITED,
};

enum vm_credits {
#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	DOM0_CREDITS = 1,
	VM1_CREDITS  = 1,
	VM2_CREDITS  = 1,
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	DOM0_CREDITS = 0, // 0 to not have credit based execution.. 
	VM1_CREDITS  = 1,
	VM2_CREDITS  = 4,
#endif
};

enum {
	VM_CAPTBL_SELF_EXITTHD_BASE    = BOOT_CAPTBL_FREE,
#if defined(__INTELLIGENT_TCAPS__)
	VM_CAPTBL_SELF_TIMEASND_BASE   = round_up_to_pow2(VM_CAPTBL_SELF_EXITTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IOTHD_BASE      = round_up_to_pow2(VM_CAPTBL_SELF_TIMEASND_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IOTCAP_BASE     = round_up_to_pow2(VM_CAPTBL_SELF_IOTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IORCV_BASE      = round_up_to_pow2(VM_CAPTBL_SELF_IOTCAP_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	VM_CAPTBL_SELF_IOTHD_BASE      = round_up_to_pow2(VM_CAPTBL_SELF_EXITTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IORCV_BASE      = round_up_to_pow2(VM_CAPTBL_SELF_IOTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
#endif
	VM_CAPTBL_SELF_IOASND_BASE     = round_up_to_pow2(VM_CAPTBL_SELF_IORCV_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_LAST_CAP             = round_up_to_pow2(VM_CAPTBL_SELF_IOASND_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_FREE                 = round_up_to_pow2(VM_CAPTBL_LAST_CAP, CAPMAX_ENTRY_SZ),
};

enum {
	VM0_CAPTBL_SELF_IOTHD_SET_BASE      = VM_CAPTBL_SELF_IOTHD_BASE, 
#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	VM0_CAPTBL_SELF_INITASND_SET_BASE   = round_up_to_pow2(VM0_CAPTBL_SELF_IOTHD_SET_BASE + CAP16B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ), 
	VM0_CAPTBL_SELF_IOTCAP_SET_BASE     = round_up_to_pow2(VM0_CAPTBL_SELF_INITASND_SET_BASE + CAP64B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ), 
	VM0_CAPTBL_SELF_IORCV_SET_BASE      = round_up_to_pow2(VM0_CAPTBL_SELF_IOTCAP_SET_BASE + CAP16B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ), 
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	VM0_CAPTBL_SELF_IORCV_SET_BASE      = round_up_to_pow2(VM0_CAPTBL_SELF_IOTHD_SET_BASE + CAP16B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ), 
#endif
	VM0_CAPTBL_SELF_IOASND_SET_BASE     = round_up_to_pow2(VM0_CAPTBL_SELF_IORCV_SET_BASE + CAP64B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ),
#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	VM0_CAPTBL_SELF_SLATHD_BASE         = round_up_to_pow2(VM0_CAPTBL_SELF_IOASND_SET_BASE + CAP64B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_SELF_SLATCAP_BASE        = round_up_to_pow2(VM0_CAPTBL_SELF_SLATHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_SELF_SLARCV_BASE         = round_up_to_pow2(VM0_CAPTBL_SELF_SLATCAP_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_SELF_SLASND_BASE         = round_up_to_pow2(VM0_CAPTBL_SELF_SLARCV_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_LAST_CAP                 = round_up_to_pow2(VM0_CAPTBL_SELF_SLASND_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ),
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	VM0_CAPTBL_LAST_CAP                 = round_up_to_pow2(VM0_CAPTBL_SELF_IOASND_SET_BASE + CAP64B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ),
#endif
	VM0_CAPTBL_FREE                     = round_up_to_pow2(VM0_CAPTBL_LAST_CAP, CAPMAX_ENTRY_SZ),
};

extern unsigned int cycs_per_usec;

extern cycles_t dom0_sla_act_cyc;
extern tcap_res_t dom0_sla_budget;

#endif /* VK_TYPES_H */
