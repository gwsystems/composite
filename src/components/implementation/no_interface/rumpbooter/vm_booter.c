#include "vk_types.h"
#include "micro_booter.h"
#include "vk_api.h"
#include "cos2rk_types.h"
#include "cos_sync.h"

extern void rump_booter_init(void);
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
int vmid;
int rumpns_vmid;

void
vm_init(void *d)
{
	int         rcvd, blocked;
	cycles_t    cycles;
	thdid_t     tid;
	tcap_time_t timeout = 0, thd_timeout;

	vmid = cos_sinv(VM_CAPTBL_SELF_SINV_BASE, VK_SERV_VM_ID << 16 | cos_thdid(), 0, 0, 0);
	rumpns_vmid = vmid;

	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
	                  (vaddr_t)cos_get_heap_ptr(), vmid == 0 ? DOM0_CAPTBL_FREE : VM_CAPTBL_FREE, &booter_info);
	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	PRINTC("Virtual-machine booter started.\n");
	rump_booter_init();
	PRINTC("Virtual-machine booter done.\n");

	cos_sinv(VM_CAPTBL_SELF_SINV_BASE, VK_SERV_VM_EXIT << 16 | cos_thdid(), 0, 0, 0);
	/* should not be scheduled. but it may be activated if this tcap or it's child tcap has budget! */
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
