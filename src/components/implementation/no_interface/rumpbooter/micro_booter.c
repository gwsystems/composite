#include "cos2rk_types.h"
#include "micro_booter.h"
#include <llprint.h>
#include "cos_sync.h"

struct cos_compinfo booter_info;
thdcap_t termthd = VM_CAPTBL_SELF_EXITTHD_BASE;	/* switch to this to shutdown */
unsigned long tls_test[TEST_NTHDS];

/* For Div-by-zero test */
int num = 1, den = 0;

/* virtual machine id */
int vmid;
int rumpns_vmid;

void
start(void)
{
	assert(0);
}

void
vm_init(void *id)
{
	vmid = (int)id;
	rumpns_vmid = vmid;
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS2RK_VIRT_MACH_MEM_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	if (id == 0) { 
		cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
				  (vaddr_t)cos_get_heap_ptr(), DOM0_CAPTBL_FREE, &booter_info);
	}
	else {
		cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
				  (vaddr_t)cos_get_heap_ptr(), VM_CAPTBL_FREE, &booter_info);
	}

	PRINTC("rump_booter_init\n");
	rump_booter_init();

	EXIT();
	return;
}

#if defined(__INTELLIGENT_TCAPS__)
void
vk_time_fn(void *d) 
{
	while (1) {
		int pending = cos_rcv(vk_time_rcv[(int)d], 0, NULL);
		printc("vkernel: rcv'd from vm %d\n", (int)d);
	}
}

void
vm_time_fn(void *d)
{
	while (1) {
		int pending = cos_rcv(VM_CAPTBL_SELF_TIMERCV_BASE, 0, NULL);
		printc("%d: rcv'd from vkernel\n", (int)d);
	}
}
#endif

extern int vmid;

void
dom0_io_fn(void *d) 
{
	int line;
	unsigned int irqline;
	arcvcap_t rcvcap;

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

	rcvcap = DOM0_CAPTBL_SELF_IORCV_SET_BASE + (((int)d - 1) * CAP64B_IDSZ);
	while (1) {
		int pending = cos_rcv(rcvcap, 0, NULL);
		intr_start(irqline);
		bmk_isr(line);
		cos_vio_tcap_set((int)d);
		intr_end();
	}
}

void
vm_io_fn(void *d)
{
	while (1) {
		int pending = cos_rcv(VM_CAPTBL_SELF_IORCV_BASE, 0, NULL);
		intr_start(IRQ_DOM0_VM);
		bmk_isr(12);
		intr_end();
	}
}

