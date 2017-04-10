#include "vk_types.h"
#include "micro_booter.h"

struct cos_compinfo booter_info;
thdcap_t termthd = VM_CAPTBL_SELF_EXITTHD_BASE;	/* switch to this to shutdown */
unsigned long tls_test[TEST_NTHDS];

static void
cos_llprint(char *s, int len)
{ call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0); }

int
prints(char *s)
{
	int len = strlen(s);

	cos_llprint(s, len);

	return len;
}

int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	  char s[128];
	  va_list arg_ptr;
	  int ret, len = 128;

	  va_start(arg_ptr, fmt);
	  ret = vsnprintf(s, len, fmt, arg_ptr);
	  va_end(arg_ptr);
	  cos_llprint(s, ret);

	  return ret;
}

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


	if(id == DL_VM) dl_booter_init();
	else rump_booter_init();

	EXIT();
	return;
}
