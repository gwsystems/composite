#include "micro_booter.h"
#include "rumpcalls.h"
#include "vkern_api.h"

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
int rumpns_vmid;

void
vm_init(void *id)
{
	int ret;
	struct cos_shm_rb *sm_rb;
	struct cos_shm_rb *sm_rb_r;

	cos_spdid_set((int)id);
	/* FIXME remove rumpns_vmid, replace with cos_spdid_get() calls */
	rumpns_vmid = (int)id;

	printc("vm_init, setting spdid for user component to: %d\n", cos_spdid_get());

	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS_VIRT_MACH_MEM_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), VM0_CAPTBL_FREE, &booter_info);


	printc("\n************ USERSPACE *************\n");

	printc("Running fs test\n");
	cos_fs_test();
	printc("Done\n");

	printc("Running shared memory test\n");
	cos_shmem_test();
	printc("Done\n");

	/* Done, just spin */
	printc("\n************ USERSPACE DONE ************\n");
	while (1);
}

void
kernel_init(void *id)
{
	int ret;
	struct cos_shm_rb *sm_rb;
	struct cos_shm_rb *sm_rb_r;

	cos_spdid_set((int)id);
	printc("vm_init: &booter_info: %p\n", &booter_info);

	printc("Kernel_init, setting spdid for kernel component to: %d\n", cos_spdid_get());
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS_VIRT_MACH_MEM_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), VM0_CAPTBL_FREE, &booter_info);

	printc("\n************ KERNEL *************\n");

	printc("Before booting rk, test sinv capabilities down to booter for shdmem api\n");
	// 12 tests
	shmem_allocate_invoke();
	shmem_allocate_invoke();
	shmem_allocate_invoke();
	shmem_allocate_invoke();
	shmem_allocate_invoke();
	shmem_allocate_invoke();
	shmem_allocate_invoke();
	shmem_allocate_invoke();
	shmem_allocate_invoke();
	shmem_allocate_invoke();
	shmem_allocate_invoke();
	shmem_allocate_invoke();
	shmem_deallocate_invoke();
	shmem_map_invoke();
	printc("\nDone\n");

	rump_booter_init();

	printc("\n************ KERNEL DONE ************\n");
}
