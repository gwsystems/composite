#include "vk_types.h"
#include "micro_booter.h"
#include "vk_api.h"

struct cos_compinfo booter_info;
thdcap_t termthd = VM_CAPTBL_SELF_EXITTHD_BASE;	/* switch to this to shutdown */
unsigned long tls_test[TEST_NTHDS];

#include <llprint.h>

/* For Div-by-zero test */
int num = 1, den = 0;

/* virtual machine id */
int vmid;

void
vm_init(void *d)
{
	int rcvd, blocked;
	cycles_t cycles;
	thdid_t tid;

	vmid = (int)d;
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), vmid == 0 ? DOM0_CAPTBL_FREE : VM_CAPTBL_FREE, &booter_info);

	PRINTC("Micro Booter started.\n");
	test_run_vk();
	PRINTC("Micro Booter done.\n");

	while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, RCV_NON_BLOCKING | RCV_ALL_PENDING, 
			     &rcvd, &tid, &blocked, &cycles) > 0) ;	
}

void
dom0_io_fn(void *id)
{
	arcvcap_t rcvcap = dom0_vio_rcvcap((unsigned int)id);
	while (1) {
		cos_rcv(rcvcap, 0, NULL);
	}
}

void
vm_io_fn(void *id)
{
	arcvcap_t rcvcap = VM_CAPTBL_SELF_IORCV_BASE;
	while (1) {
		cos_rcv(rcvcap, 0, NULL);
	}
}
