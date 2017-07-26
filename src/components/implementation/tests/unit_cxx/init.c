#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include "init.h"

struct cos_compinfo parent_cinfo;

void init_cinfo(void){
	cos_meminfo_init(&parent_cinfo.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&parent_cinfo, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
					(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &parent_cinfo);
}
