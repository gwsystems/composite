#ifndef COS2RK_TYPES_H
#define COS2RK_TYPES_H

#include "vk_types.h"

#define HW_ISR_LINES 32
#define HW_ISR_FIRST 1

#define IRQ_DOM0_VM 22 /* DOM0's message line to VM's, in VM's */
#define IRQ_VM1 21     /* VM1's message line to DOM0, so in DOM0 */
#define IRQ_VM2 27     /* VM2's message line to DOM0, so in DOM0 */

#define COS2RK_MEM_SHM_BASE VK_VM_SHM_BASE
#define COS2RK_VIRT_MACH_COUNT VM_COUNT
#define COS2RK_VIRT_MACH_MEM_SZ VM_UNTYPED_SIZE //128MB
#define COS2RK_SHM_VM_SZ VM_SHM_SZ //4MB
#define COS2RK_SHM_ALL_SZ VM_SHM_ALL_SZ

#undef PRINT_CPU_USAGE 
#define MIN_CYCS (1<<12)

#define BOOTUP_ITERS 100 

capid_t irq_thdcap[HW_ISR_LINES]; 
thdid_t irq_thdid[HW_ISR_LINES];
tcap_t irq_tcap[HW_ISR_LINES]; 
capid_t irq_arcvcap[HW_ISR_LINES];
tcap_prio_t irq_prio[HW_ISR_LINES];

enum vm_prio {
	PRIO_BOOST = TCAP_PRIO_MAX,
	PRIO_OVER  = TCAP_PRIO_MAX + 100,
	PRIO_UNDER = TCAP_PRIO_MAX + 50,
};

enum vm_credits {
	DOM0_CREDITS = 5, // not used, DOM0 gets INF budget.. But this is required for cpu usage calc. (assuming dom0 is 50% & vm1 + vm2 = 50%) 
	VM1_CREDITS  = 4,
	VM2_CREDITS  = 1,
};

enum {
	VM_CAPTBL_SELF_EXITTHD_BASE    = BOOT_CAPTBL_FREE,
	VM_CAPTBL_SELF_IOTHD_BASE      = round_up_to_pow2(VM_CAPTBL_SELF_EXITTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IORCV_BASE      = round_up_to_pow2(VM_CAPTBL_SELF_IOTHD_BASE + CAP16B_IDSZ, CAPMAX_ENTRY_SZ), 
	VM_CAPTBL_SELF_IOASND_BASE     = round_up_to_pow2(VM_CAPTBL_SELF_IORCV_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_LAST_CAP             = round_up_to_pow2(VM_CAPTBL_SELF_IOASND_BASE + CAP64B_IDSZ, CAPMAX_ENTRY_SZ),
	VM_CAPTBL_FREE                 = round_up_to_pow2(VM_CAPTBL_LAST_CAP, CAPMAX_ENTRY_SZ),
};

enum {
	DOM0_CAPTBL_SELF_IOTHD_SET_BASE      = VM_CAPTBL_SELF_IOTHD_BASE, 
	DOM0_CAPTBL_SELF_IORCV_SET_BASE      = round_up_to_pow2(DOM0_CAPTBL_SELF_IOTHD_SET_BASE + CAP16B_IDSZ*(COS2RK_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ), 
	DOM0_CAPTBL_SELF_IOASND_SET_BASE     = round_up_to_pow2(DOM0_CAPTBL_SELF_IORCV_SET_BASE + CAP64B_IDSZ*(COS2RK_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ),
	DOM0_CAPTBL_LAST_CAP                 = round_up_to_pow2(DOM0_CAPTBL_SELF_IOASND_SET_BASE + CAP64B_IDSZ*(COS2RK_VIRT_MACH_COUNT-1), CAPMAX_ENTRY_SZ),
	DOM0_CAPTBL_FREE                     = round_up_to_pow2(DOM0_CAPTBL_LAST_CAP, CAPMAX_ENTRY_SZ),
};

extern unsigned int cycs_per_usec;

#endif /* COS2RK_TYPES_H */
