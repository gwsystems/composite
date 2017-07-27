#include "vk_types.h"
#include "micro_booter.h"
#include <llprint.h>

struct cos_compinfo booter_info;
thdcap_t termthd = VM_CAPTBL_SELF_EXITTHD_BASE;	/* switch to this to shutdown */
unsigned long tls_test[TEST_NTHDS];

/* For Div-by-zero test */
int num = 1, den = 0;

/* virtual machine id */
int vmid;
int rumpns_vmid;

void
vm_init(void *id)
{
	vmid = (int)id;
	rumpns_vmid = vmid;
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS_VIRT_MACH_MEM_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	if (id == 0) { 
		cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
				  (vaddr_t)cos_get_heap_ptr(), VM0_CAPTBL_FREE, &booter_info);
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
