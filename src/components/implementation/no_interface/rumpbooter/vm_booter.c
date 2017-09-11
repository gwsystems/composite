#include "vk_types.h"
#include "micro_booter.h"
#include "vk_api.h"
#include "cos2rk_types.h"
#include "cos_sync.h"
#include "rumpcalls.h"
#include <sl.h>

extern void rump_booter_init(void);
extern void timer_comp_init(void *);
extern void dlapp_init(void *);
extern void rk_kernel_init(void *);
struct cos_compinfo booter_info;
/*
 * the capability for the thread switched to upon termination.
 * FIXME: not exit thread for now
 */
thdcap_t      termthd = BOOT_CAPTBL_SELF_INITTHD_BASE;
unsigned long tls_test[TEST_NTHDS];
unsigned int  cycs_per_usec;

#include <llprint.h>

/* For Div-by-zero test */
int num = 1, den = 0;

/* virtual machine id */
int vmid = 0;
int rumpns_vmid;

extern int cos_shmem_test(void);

cycles_t
hpet_first_period(void)
{
	int ret;
	cycles_t start_period = 0;

	while ((ret = cos_introspect64(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_GET_FIRST_HPET, &start_period)) == -EAGAIN) ;
	if (ret) assert(0);

	return start_period;
}

void
vm_init(void *unused)
{
	int         rcvd, blocked;
	cycles_t    cycles;
	thdid_t     tid;
	tcap_time_t timeout = 0, thd_timeout;
	arcvcap_t rcvcap = BOOT_CAPTBL_SELF_INITRCV_BASE;

	vmid = vk_vm_id();

	switch(vmid) {
	case RUMP_SUB: 
		rk_kernel_init(NULL);

		assert(0);
		break;
	case TIMER_SUB:
		timer_comp_init(NULL);

		assert(0);
		break;
	case DL_APP:
		dlapp_init(NULL);

		assert(0);
		break;
	case UDP_APP: break;
	default: assert(0);
	}

	rumpns_vmid = vmid;
	cos_spdid_set(vmid);

	PRINTC("\n******************* USERSPACE *******************\n");

	PRINTC("vm_init, setting spdid for user component to: %d\n", vmid);
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
	                  (vaddr_t)cos_get_heap_ptr(), vmid == 0 ? DOM0_CAPTBL_FREE : VM_CAPTBL_FREE, &booter_info);

	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	/* Userspace component >= 2 then spin */
	if (vmid > 2) {
		PRINTC("Userspace Component #%d spinning...\n", vmid);
		goto done;
	}

	PRINTC("Virtual-machine booter started.\n");
	/* FIXME, we need to wait till the RK is done booting to do this */
	//PRINTC("Running fs test\n");
	//cos_fs_test();
	//PRINTC("Done\n");

	PRINTC("Running shared memory tests\n");
	cos_shmem_test();
	PRINTC("Virtual-machine booter done.\n");

done:
	PRINTC("\n******************* USERSPACE DONE *******************\n");
	//cos_sinv(VM_CAPTBL_SELF_SINV_BASE, VK_SERV_VM_EXIT << 16 | vmid, 0, 0, 0);
	while (1) {
		cos_rcv(rcvcap, 0, NULL);
	}
	/* should not be scheduled. but it may be activated if this tcap or it's child tcap has budget! */
}

void
rk_kernel_init(void *unused)
{
	int ret;
	struct cos_shm_rb *sm_rb;
	struct cos_shm_rb *sm_rb_r;

	PRINTC("\n******************* KERNEL *******************\n");

	rumpns_vmid = vmid;
	assert(vmid < VM_COUNT);
	cos_spdid_set(vmid);

	PRINTC("Kernel_init, setting spdid for kernel component to: %d\n", vmid);
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), DOM0_CAPTBL_FREE, &booter_info);

	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	rump_booter_init();

	PRINTC("\n******************* KERNEL DONE *******************\n");
	vk_vm_exit();
}

void
dom0_io_fn(void *d)
{
	int          line;
	unsigned int irqline;
	arcvcap_t    rcvcap = dom0_vio_rcvcap((unsigned int)d);

	switch((int)d) {
		case 1:
			line = 13;
			irqline = IRQ_VM1;
			break;
		case 2:
			line = 15;
			irqline = IRQ_VM2;
			break;
		default: assert(0);
	}

	while (1) {
		cos_rcv(rcvcap, 0, NULL);
		intr_start(irqline);
		bmk_isr(line);
		cos_vio_tcap_set((int)d);
		intr_end();
	}
}

void
vm_io_fn(void *id)
{
	arcvcap_t rcvcap = VM_CAPTBL_SELF_IORCV_BASE;

	while (1) {
		cos_rcv(rcvcap, 0, NULL);
		intr_start(IRQ_DOM0_VM);
		bmk_isr(12);
		intr_end();

	}
}
