#include "vk_api.h"
#include "vk_types.h"

extern struct vms_info vmx_info[];
extern struct vkernel_info vk_info;
static unsigned int ready_vms = VM_COUNT;

void
vm_exit(void *d)
{
        printc("%d: EXIT\n", (int)d);
        ready_vms --;
        vmx_info[(int)d].state = VM_EXITED;

        while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
}

void
scheduler(void)
{
        static unsigned int i;
        thdid_t             tid;
        int                 blocked;
        cycles_t            cycles;
        int                 index;

        while (ready_vms) {
                index = i++ % VM_COUNT;

                if (vmx_info[index].state == VM_RUNNING) {
                        assert(vk_info.vminitasnd[index]);

                        if (cos_tcap_delegate(vk_info.vminitasnd[index], BOOT_CAPTBL_SELF_INITTCAP_BASE,
                                              VM_BUDGET_FIXED, VM_PRIO_FIXED, TCAP_DELEG_YIELD)) assert(0);
                }

                while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, 0, NULL, &tid, &blocked, &cycles)) ;
        }
}

