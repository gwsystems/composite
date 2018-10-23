#include <cos_kernel_api.h>
#include <pong.h>
#include <cos_types.h>
#include <cobj_format.h>
#include <hypercall.h>
#include <cos_rdtsc.h>

#define ITERS 1000000

struct cos_compinfo booter_info;

static cycles_t inv_cycles[ITERS] = { 0 };
static cycles_t inv_only[ITERS] = { 0 };
static cycles_t ret_only[ITERS] = { 0 };
static cycles_t rdtscp_min = 0, rdtscp_avg = 0, rdtscp_max = 0;

//#define UBENCH_INVFULL
#define UBENCH_INVONLY
#define UBENCH_RETONLY
#define UBENCH_INVBASIC

static void
test_inv_full(void)
{
	int i;

	call_args(0, 0, 0, 0);
        cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_CACHE_FLUSH, 0);
	for (i = 0; i < ITERS; i++) {
		cycles_t st, en, mid;
		unsigned long r1 = 0, r2 = 0;

//		rdtscll(st);
		cos_rdtscp(st);
#if defined(UBENCH_INVONLY) || defined(UBENCH_RETONLY)
		call_3rets(&r1, &r2, 1, 0, 0, 0);
#else
		call_3rets(&r1, &r2, 0, 0, 0, 0);
#endif
//		rdtscll(en);
		cos_rdtscp(en);
		mid = ((cycles_t)r1) << 32 | ((cycles_t)r2);

#ifdef UBENCH_INVONLY
		inv_only[i] = mid - st - rdtscp_min;
#endif
#ifdef UBENCH_RETONLY
		ret_only[i] = en - mid - rdtscp_min;
#endif
#ifdef UBENCH_INVFULL
		inv_cycles[i] = en - st - (2*rdtscp_min);
#endif
	}

#ifdef UBENCH_INVFULL
	printc("invocation full (with rets): \n");
	for (i = 0; i < ITERS; i++) {
		printc("%llu\n", inv_cycles[i]);
	}
	printc("------------------\n");
#endif
#ifdef UBENCH_INVONLY
	printc("invocation only (with rets): \n");
	for (i = 0; i < ITERS; i++) {
		printc("%llu\n", inv_only[i]);
	}
	printc("------------------\n");
#endif
#ifdef UBENCH_RETONLY
	printc("return only (with rets): \n");
	for (i = 0; i < ITERS; i++) {
		printc("%llu\n", ret_only[i]);
	}
	printc("------------------\n");
#endif
}

static void
test_inv(void)
{
	int i;

	call_args(0, 0, 0, 0);
        cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITHW_BASE, HW_CACHE_FLUSH, 0);
	for (i = 0; i < ITERS; i++) {
		cycles_t st, en;

		rdtscll(st);
		call_args(0, 0, 0, 0);
		rdtscll(en);

		inv_cycles[i] = en - st;
	}

#ifdef UBENCH_INVBASIC
	printc("invocation full (without rets): \n");
	for (i = 0; i < ITERS; i++) {
		printc("%llu\n", inv_cycles[i]);
	}
	printc("------------------\n");
#endif
}

void cos_init(void)
{
	/* just for HWflush call */
	/* do not try to alloc resources using this booter_info */
	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);

//	int r = 0, r1 = 0, r2 = 0;
//	int a = 10, b = 20, c = 30, d = 40;
//
//	PRINTLOG(PRINT_DEBUG, "Welcome to the ping component\n");
//
//	PRINTLOG(PRINT_DEBUG, "Invoking pong interface:\n");
//	r = call();
//	assert(r == 0);
//	r = call_two();
//	assert(r == 2);
//	r = call_three();
//	assert(r == 3);
//	r = call_four();
//	assert(r == 4);
//
//	PRINTLOG(PRINT_DEBUG, "Invoking pong interface w/ arguments:\n");
//	r = call_arg(a);
//	assert(r == a);
//	r = call_args(a, b, c, d);
//	assert(r == a);
//
//	PRINTLOG(PRINT_DEBUG, "Invoking pong interface w/ multiple-rets:\n");
//	r = call_3rets(&r1, &r2, a, b, c, d);
//	assert(r == a);
//	PRINTLOG(PRINT_DEBUG, "Returns=> r1: %d, r2: %d\n", r1, r2);
//	assert(r1 == (a + b + c + d));
//	assert(r2 == (a - b - c - d));
	cos_rdtscp_calib(&rdtscp_min, &rdtscp_avg, &rdtscp_max);

	test_inv_full();
	test_inv();
	hypercall_comp_init_done();

	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}
