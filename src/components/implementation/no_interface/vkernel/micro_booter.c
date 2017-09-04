#include "vk_types.h"
#include "micro_booter.h"
#include "vk_api.h"

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



void
vm_init(void *d)
{
	/* FIXME vmid is no longer continued, see rumpcalls.h The call to cos_spdid_get should be in different api */
	int vmid_discontinued = -1;
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, VM_UNTYPED_SIZE, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), vmid_discontinued == 0 ? DOM0_CAPTBL_FREE : VM_CAPTBL_FREE, &booter_info);

	PRINTC("Micro Booter started.\n");
	test_run_vk();
	PRINTC("Micro Booter done.\n");

	EXIT();
	return;
}

void
dom0_io_fn(void *id)
{
	arcvcap_t rcvcap = dom0_vio_rcvcap((unsigned int)id);
	while (1) {
		int pending = cos_rcv(rcvcap);
	}
}

void
vm_io_fn(void *id)
{
	arcvcap_t rcvcap = VM_CAPTBL_SELF_IORCV_BASE;
	while (1) {
		int pending = cos_rcv(rcvcap);
	}
}
