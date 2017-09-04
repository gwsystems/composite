#ifndef VK_TYPES_H
#define VK_TYPES_H

#include <cos_types.h>
#include <cos_kernel_api.h>

#define VM_COUNT 3
#define VM_UNTYPED_SZ (1<<27) //128MB
#define COS_SHM_VM_SZ (1<<20) //2MB
#define COS_SHM_ALL_SZ (((VM_COUNT - 1) > 0 ? (VM_COUNT - 1) : 1) * COS_SHM_VM_SZ) //shared regions with VM 0

#define CPU_VM 9
#define DL_VM 2

#define VM_MS_TIMESLICE 1
#define VM_TIMESLICE (VM_MS_TIMESLICE*1000) //*cycs_per_usec = 1ms

#define HW_ISR_LINES 32
#define HW_ISR_FIRST 0

#define VK_VM_SHM_BASE 0x80000000
#define VM_SHM_SZ      (1<<20)
#define VM_SHM_ALL_SZ  ((VM_COUNT>0)?(VM_COUNT*VM_SHM_SZ):VM_SHM_SZ)

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

//#define __SIMPLE_DOM0_HIGH__
#define __SIMPLE_DOM0_LOW__

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

/* ------ End of Items from vkernel/vk_types.h ------ */

enum vm_status {
	VM_RUNNING = 0,
	VM_BLOCKED = 1,
	VM_EXPENDED = 2,
	VM_EXITED = 3,
};

enum vm_credits {
	DOM0_CREDITS = 5,
	DOM0_PERIOD  = 10,
	DOM0_WKUP_PERIOD = 5,

	VM1_CREDITS  = 2,
	VM1_PERIOD   = 10,
	VM1_WKUP_PERIOD = 10,

	VM2_CREDITS  = 5,
	VM2_PERIOD   = 10,
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
	VM0_CAPTBL_SELF_IORCV_SET_BASE      = round_up_to_pow2(VM0_CAPTBL_SELF_IOTHD_SET_BASE + CAP16B_IDSZ*(VM_COUNT-1), CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_SELF_IOASND_SET_BASE     = round_up_to_pow2(VM0_CAPTBL_SELF_IORCV_SET_BASE + CAP64B_IDSZ*(VM_COUNT-1), CAPMAX_ENTRY_SZ),
	/* RG, I need a place where the vkernel can copy in to vm0 a sinv to vmx */
	VM0_CAPTBL_SELF_IOSINV_BASE         = round_up_to_pow2(VM0_CAPTBL_SELF_IOASND_SET_BASE + CAP32B_IDSZ, CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_SELF_IOSINV_ALLOC        = round_up_to_pow2(VM0_CAPTBL_SELF_IOSINV_BASE + CAP32B_IDSZ, CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_SELF_IOSINV_DEALLOC      = round_up_to_pow2(VM0_CAPTBL_SELF_IOSINV_ALLOC + CAP32B_IDSZ, CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_SELF_IOSINV_MAP          = round_up_to_pow2(VM0_CAPTBL_SELF_IOSINV_DEALLOC + CAP32B_IDSZ, CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_SELF_IOSINV_TEST	    = round_up_to_pow2(VM0_CAPTBL_SELF_IOSINV_MAP + CAP32B_IDSZ, CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_SELF_IOSINV_VADDR_GET    = round_up_to_pow2(VM0_CAPTBL_SELF_IOSINV_TEST + CAP32B_IDSZ, CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_LAST_CAP                 = round_up_to_pow2(VM0_CAPTBL_SELF_IOSINV_VADDR_GET + CAP64B_IDSZ*(VM_COUNT-1), CAPMAX_ENTRY_SZ),
	VM0_CAPTBL_FREE                     = round_up_to_pow2(VM0_CAPTBL_LAST_CAP, CAPMAX_ENTRY_SZ),
};

enum dom0_captbl_layout {
	/* DOM0 I/O Capabilities layout */
        DOM0_CAPTBL_SELF_IOTHD_SET_BASE      = VM_CAPTBL_SELF_IOTHD_BASE,
        DOM0_CAPTBL_SELF_IOTCAP_SET_BASE     = round_up_to_pow2(DOM0_CAPTBL_SELF_IOTHD_SET_BASE + CAP16B_IDSZ*((VM_COUNT>1 ? VM_COUNT-1 : 1)), CAPMAX_ENTRY_SZ),
        DOM0_CAPTBL_SELF_IORCV_SET_BASE      = round_up_to_pow2(DOM0_CAPTBL_SELF_IOTCAP_SET_BASE + CAP16B_IDSZ*((VM_COUNT>1 ? VM_COUNT-1 : 1)), CAPMAX_ENTRY_SZ),
        DOM0_CAPTBL_SELF_IOASND_SET_BASE     = round_up_to_pow2(DOM0_CAPTBL_SELF_IORCV_SET_BASE + CAP64B_IDSZ*((VM_COUNT>1 ? VM_COUNT-1 : 1)), CAPMAX_ENTRY_SZ),
        DOM0_CAPTBL_SELF_LAST_CAP            = DOM0_CAPTBL_SELF_IOASND_SET_BASE + CAP64B_IDSZ*((VM_COUNT>1 ? VM_COUNT-1 : 1)),
	DOM0_CAPTBL_FREE                     = round_up_to_pow2(DOM0_CAPTBL_SELF_LAST_CAP, CAPMAX_ENTRY_SZ),
};

extern unsigned int cycs_per_usec;
extern unsigned int cycs_per_msec;

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

struct vm_io_info {
        thdcap_t iothd;
        arcvcap_t iorcv;
        asndcap_t ioasnd;
};

struct dom0_io_info {
        thdcap_t iothds[VM_COUNT-1];
        tcap_t iotcaps[VM_COUNT-1];
        arcvcap_t iorcvs[VM_COUNT-1];
        asndcap_t ioasnds[VM_COUNT-1];
};

#endif /* VK_TYPES_H */
