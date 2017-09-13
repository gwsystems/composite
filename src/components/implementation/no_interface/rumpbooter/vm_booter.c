#include "vk_types.h"
#include "micro_booter.h"
#include "vk_api.h"
#include "cos2rk_types.h"
#include "cos_sync.h"
#include "rumpcalls.h"
#include "rk_inv_api.h"
#include <sl.h>

extern void rump_booter_init(void);
extern void timer_comp_init(void *);
extern void dlapp_init(void *);
extern void rk_kernel_init(void *);
extern int main(void);
struct cos_compinfo booter_info;
/*
 * the capability for the thread switched to upon termination.
 * FIXME: not exit thread for now
 */
thdcap_t      termthd = BOOT_CAPTBL_SELF_INITTHD_BASE;
unsigned long tls_test[TEST_NTHDS];

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

	vmid = vk_vm_id();
	printc("!!!!!vmid: %d\n", vmid);

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

	assert(vmid == UDP_APP);
	PRINTC("\n******************* USERSPACE *******************\n");

	PRINTC("vm_init, setting spdid for user component to: %d\n", vmid);
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
	                  (vaddr_t)cos_get_heap_ptr(), APP_CAPTBL_FREE, &booter_info);


	PRINTC("Running shared memory tests\n");
	cos_shmem_test();
	PRINTC("Virtual-machine booter done.\n");

	PRINTC("Running udp app\n");
	main();
	PRINTC("Done running udp app\n");

	PRINTC("\n******************* USERSPACE DONE *******************\n");
	/* Perhaps platform block from BMK? But this should not happen!!*/
	while (1) ;
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
			(vaddr_t)cos_get_heap_ptr(), VM_CAPTBL_FREE, &booter_info);

	rump_booter_init();

	PRINTC("\n******************* KERNEL DONE *******************\n");
	vk_vm_exit();
}

void
dom0_io_fn(void *d)
{
	assert(0);
//	int          line;
//	unsigned int irqline;
//	arcvcap_t    rcvcap = dom0_vio_rcvcap((unsigned int)d);
//
//	switch((int)d) {
//		case 1:
//			line = 13;
//			irqline = IRQ_VM1;
//			break;
//		case 2:
//			line = 15;
//			irqline = IRQ_VM2;
//			break;
//		default: assert(0);
//	}
//
//	while (1) {
//		cos_rcv(rcvcap, 0, NULL);
//		intr_start(irqline);
//		bmk_isr(line);
//		cos_vio_tcap_set((int)d);
//		intr_end();
//	}
}

void
vm_io_fn(void *id)
{
	assert(0);
//	arcvcap_t rcvcap = VM_CAPTBL_SELF_IORCV_BASE;
//
//	while (1) {
//		cos_rcv(rcvcap, 0, NULL);
//		intr_start(IRQ_DOM0_VM);
//		bmk_isr(12);
//		intr_end();
//
//	}
}
