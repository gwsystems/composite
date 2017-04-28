#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <cos_types.h>

#define COS_VIRT_MACH_COUNT 3
#define COS_VIRT_MACH_MEM_SZ (1<<27) //128MB
#define COS_SHM_VM_SZ (1<<20) //2MB
#define COS_SHM_ALL_SZ (((COS_VIRT_MACH_COUNT - 1) > 0 ? (COS_VIRT_MACH_COUNT - 1) : 1) * COS_SHM_VM_SZ) //shared regions with VM 0

#define DL_VM 2

#define VM_MS_TIMESLICE 1
#define VM_TIMESLICE 1*1000//*cycs_per_usec = 1ms
#define VM_MIN_TIMESLICE (10) //1us
#define SCHED_MIN_TIMESLICE (10)
#define SCHED_QUANTUM (VM_TIMESLICE * 100)

#define VK_CYCS_DIFF_THRESH (1<<8)

#undef PRINT_CPU_USAGE 
#define MIN_CYCS (1<<12)

#define BOOTUP_ITERS 100 

#define __SIMPLE_XEN_LIKE_TCAPS__

#define HW_ISR_LINES 32
#define HW_ISR_FIRST 0

capid_t irq_thdcap[HW_ISR_LINES]; 
thdid_t irq_thdid[HW_ISR_LINES];
tcap_t irq_tcap[HW_ISR_LINES]; 
capid_t irq_arcvcap[HW_ISR_LINES];
tcap_prio_t irq_prio[HW_ISR_LINES];

enum vm_prio {
	PRIO_HIGH = TCAP_PRIO_MAX,
	PRIO_LOW  = TCAP_PRIO_MAX+2,
	PRIO_MID = TCAP_PRIO_MAX+1,
};

#define HPET_PERIOD_MS 10
#define HPET_PERIOD_US (HPET_PERIOD_MS*1000)

#define __SIMPLE_DOM0_HIGH__
#undef  __SIMPLE_DOM0_LOW__

#if defined(__SIMPLE_DOM0_HIGH__)

#define DLVM_PRIO PRIO_MID
#define NWVM_PRIO PRIO_LOW
#define DOM0_PRIO PRIO_HIGH

#elif defined(__SIMPLE_DOM0_LOW__)

#define DLVM_PRIO PRIO_HIGH
#define NWVM_PRIO PRIO_MID
#define DOM0_PRIO PRIO_LOW

#endif

#define HPET_PRIO DOM0_PRIO

#define DLVM_ADD_WORK 0

enum vm_status {
	VM_RUNNING = 0,
	VM_BLOCKED = 1,
	VM_EXPENDED = 2,
	VM_EXITED = 3,
};

enum vm_credits {
	DOM0_CREDITS = 1, 
	VM1_CREDITS  = 4,
	VM2_CREDITS  = 1,
};

enum {
	VM_CAPTBL_SELF_EXITTHD_BASE    = BOOT_CAPTBL_FREE,
	VM_CAPTBL_SELF_VKASND_BASE     = round_up_to_pow2(VM_CAPTBL_SELF_EXITTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IOTHD_BASE      = round_up_to_pow2(VM_CAPTBL_SELF_VKASND_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IORCV_BASE      = round_up_to_pow2(VM_CAPTBL_SELF_IOTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IOASND_BASE     = round_up_to_pow2(VM_CAPTBL_SELF_IORCV_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_LAST_CAP             = round_up_to_pow2(VM_CAPTBL_SELF_IOASND_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_FREE                 = round_up_to_pow2(VM_CAPTBL_LAST_CAP, CAPMAX_ENTRY_SZ),
};

enum {
	VM0_CAPTBL_SELF_IOTHD_SET_BASE      = VM_CAPTBL_SELF_IOTHD_BASE, 
	VM0_CAPTBL_SELF_IORCV_SET_BASE      = round_up_to_pow2(VM0_CAPTBL_SELF_IOTHD_SET_BASE + CAP16B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ), 
	VM0_CAPTBL_SELF_IOASND_SET_BASE     = round_up_to_pow2(VM0_CAPTBL_SELF_IORCV_SET_BASE + CAP64B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_LAST_CAP                 = round_up_to_pow2(VM0_CAPTBL_SELF_IOASND_SET_BASE + CAP64B_IDSZ*(COS_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_FREE                     = round_up_to_pow2(VM0_CAPTBL_LAST_CAP, CAPMAX_ENTRY_SZ),
};

extern unsigned int cycs_per_usec;
extern unsigned int cycs_per_msec;

#endif /* VK_TYPES_H */
