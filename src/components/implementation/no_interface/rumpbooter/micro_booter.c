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

cycles_t
hpet_first_period(void)
{
	int ret;
	static cycles_t start_period = 0;

	if (start_period) return start_period;

	while ((ret = cos_introspect64(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_GET_FIRSTPERIOD, &start_period)) == -EAGAIN) ;
	if (ret) assert(0);

//	printc("sp:%llu\n", start_period);
	return start_period;
}

void
spin_forever(void)
{
	printc("CPU-BOUND VM..SPINNING FOREVER\n");

	while (1) ;
}

void
vm_init(void *id)
{
	int ret, vmid;
	struct cos_shm_rb *sm_rb;
	struct cos_shm_rb *sm_rb_r;

	printc("\n************ USERSPACE *************\n");

	cos_spdid_set((int)id);
	/* FIXME remove rumpns_vmid, replace with cos_spdid_get() calls */
        vmid = cos_spdid_get();
	assert(vmid < VM_COUNT);
	rumpns_vmid = (int)id;

	printc("vm_init, setting spdid for user component to: %d\n", cos_spdid_get());
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, VM_UNTYPED_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), VM_CAPTBL_FREE, &booter_info);

	/* Only run userspace for component id 1*/
	if (cos_spdid_get() > 1) {
		printc("Userspace Component #%d spinning...\n", cos_spdid_get());
		goto done;
	}

	/* FIXME, we need to wait till the RK is done booting to do this */
	/* TODO Add a syscall checking if a variable is set yet */
	//printc("Running fs test\n");
	//cos_fs_test();
	//printc("Done\n");

	printc("Running shared memory tests\n");
	cos_shmem_test();
	printc("Done\n");

	/* Done, just spin */
done:
	printc("\n************ USERSPACE DONE ************\n");
	while (1);
}

void
kernel_init(void *id)
{
	int ret, vmid;
	struct cos_shm_rb *sm_rb;
	struct cos_shm_rb *sm_rb_r;


	printc("\n************ KERNEL *************\n");

        cos_spdid_set((int)id);
	/* FIXME remove rumpns_vmid, replace with cos_spdid_get() calls */
        vmid = cos_spdid_get();
	assert(vmid < VM_COUNT);
	rumpns_vmid = vmid;

	printc("Kernel_init, setting spdid for kernel component to: %d\n", cos_spdid_get());
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, VM_UNTYPED_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), VM0_CAPTBL_FREE, &booter_info);

	if(id == DL_VM) dl_booter_init();
	else if (id == CPU_VM) spin_forever();
	else rump_booter_init();

	printc("\n************ KERNEL DONE ************\n");
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
