#include <stdint.h>

#include "micro_booter.h"

extern int _expect_llu(int predicate, char *str, long long unsigned a, long long unsigned b, char *errcmp, char *testname, char *file, int line);
extern int _expect_ll(int predicate, char *str, long long a, long long b, char *errcmp, char *testname, char *file, int line);

unsigned int cyc_per_usec;
static volatile arcvcap_t rcc_global[NUM_CPU], rcp_global[NUM_CPU];
static volatile asndcap_t scp_global[NUM_CPU];
static int                async_test_flag_[NUM_CPU] = { 0 };

static void
async_thd_fn_perf(void *thdcap)
{
	thdcap_t  tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcc_global[cos_cpuid()];
	int       i, ret;

	cos_rcv(rc, 0, NULL);

	for (i = 0; i < ITER + 1; i++) {
		cos_rcv(rc, 0, NULL);
		cos_thd_switch(tc);

	}

	ret = cos_thd_switch(tc);
    EXPECT_LL_NEQ(0, ret, "COS Switch Error");
}

static void
async_thd_parent_perf(void *thdcap)
{
	thdcap_t  tc = (thdcap_t)thdcap;
	asndcap_t sc = scp_global[cos_cpuid()];
	long long e	= 0, s = 0, t = 0,max = 0, min = 0;
	int       i;

	cos_asnd(sc, 1);

	for (i = 0; i < ITER; i++) {
		rdtscll(s);
		cos_asnd(sc, 1);
		rdtscll(e);

		t += e - s;

		if( e - s > max){
			max = e - s;
		}

		if( e - s < min || min == 0){
			min = e - s;
		}
	}

	PRINTC("Test Async Endpoints Rountrip Time: \t AVG:%llu, MAX:%llu, MIN:%llu, ITER:%lld\n",
		  (long long unsigned)(t / ITER), (long long unsigned) max, (long long unsigned) min, (long long) ITER);

	async_test_flag_[cos_cpuid()] = 0;
	while (1) cos_thd_switch(tc);
}

static void
test_async_endpoints_perf(void)
{
	thdcap_t  tcp, tcc;
	tcap_t    tccp, tccc;
	arcvcap_t rcp, rcc;

	/* parent rcv capabilities */
	tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent_perf,
	                    (void *)BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	if(EXPECT_LL_LT(1, tcp, "Test Async Endpoints")) return;
	tccp = cos_tcap_alloc(&booter_info);
	if(EXPECT_LL_LT(1, tccp, "Test Async Endpoints")) return;
	rcp = cos_arcv_alloc(&booter_info, tcp, tccp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
	if(EXPECT_LL_LT(1, rcp, "Test Async Endpoints")) return;
	if(EXPECT_LL_NEQ(0,cos_tcap_transfer(rcp, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF,
        TCAP_PRIO_MAX + 1), "Test Async Endpoints")) {
		return;
	}

	/* child rcv capabilities */
	tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn_perf, (void *)tcp);
	if(EXPECT_LL_LT(1, tcc, "Test Async Endpoints")) return;
	tccc = cos_tcap_alloc(&booter_info);
	if(EXPECT_LL_LT(1, tccc, "Test Async Endpoints")) return;
	rcc = cos_arcv_alloc(&booter_info, tcc, tccc, booter_info.comp_cap, rcp);
	if(EXPECT_LL_LT(1, rcc, "Test Async Endpoints")) return;
	if(EXPECT_LL_NEQ(0,cos_tcap_transfer(rcc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF,
        TCAP_PRIO_MAX), "Test Async Endpoints"))
		 return;

	/* make the snd channel to the child */
	scp_global[cos_cpuid()] = cos_asnd_alloc(&booter_info, rcc, booter_info.captbl_cap);
	if(EXPECT_LL_EQ(0, scp_global[cos_cpuid()], "Test Async Endpoints")) return;

	rcc_global[cos_cpuid()] = rcc;
	rcp_global[cos_cpuid()] = rcp;

	async_test_flag_[cos_cpuid()] = 1;
	while (async_test_flag_[cos_cpuid()]) cos_thd_switch(tcp);
}

static long long midinv_cycles[NUM_CPU] = { 0LL };

static int
test_serverfn(int a, int b, int c)
{
	rdtscll(midinv_cycles[cos_cpuid()]);
	return 0xDEADBEEF;
}

extern void *__inv_test_serverfn(int a, int b, int c);

static inline int
call_cap_mb(u32_t cap_no, int arg1, int arg2, int arg3)
{
	int ret;

	/*
	 * Which stack should we use for this invocation?  Simple, use
	 * this stack, at the current sp.  This is essentially a
	 * function call into another component, with odd calling
	 * conventions.
	 */
	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;

	__asm__ __volatile__("pushl %%ebp\n\t"
	                     "movl %%esp, %%ebp\n\t"
	                     "movl %%esp, %%edx\n\t"
	                     "movl $1f, %%ecx\n\t"
	                     "sysenter\n\t"
	                     "1:\n\t"
	                     "popl %%ebp"
	                     : "=a"(ret)
	                     : "a"(cap_no), "b"(arg1), "S"(arg2), "D"(arg3)
	                     : "memory", "cc", "ecx", "edx");

	return ret;
}

static void
test_inv_perf(void)
{
	compcap_t    cc;
	sinvcap_t    ic;
	int          i;
	long long    total_inv_cycles = 0LL, total_ret_cycles = 0LL, inv_max = 0LL, inv_min = 0LL;
	long long 	 ret_max = 0LL, ret_min = 0LL;
	long long	 time = 0LL, mask = 0LL;
	unsigned int ret;

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	if(EXPECT_LL_LT(1, cc, "Test Invocation")) return;
	ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn, 0);
	if(EXPECT_LL_LT(1, ic, "Test Invocation")) return;
	ret = call_cap_mb(ic, 1, 2, 3);
	assert(ret == 0xDEADBEEF);

	for (i = 0; i < ITER; i++) {
		long long start_cycles = 0LL, end_cycles = 0LL;

		midinv_cycles[cos_cpuid()] = 0LL;
		rdtscll(start_cycles);
		call_cap_mb(ic, 1, 2, 3);
		rdtscll(end_cycles);

		/* FAST ABS */
		time = (midinv_cycles[cos_cpuid()] - start_cycles);
		mask = (time >> (sizeof(int) * CHAR_BIT - 1));
        time = (time + mask) ^ mask;

		total_inv_cycles += time;
		if (time > inv_max){
			inv_max = time;
		}

		if (time < inv_min || inv_min == 0){
			inv_min = time;
		}

		total_ret_cycles += (end_cycles - midinv_cycles[cos_cpuid()]);
		if (end_cycles - midinv_cycles[cos_cpuid()] > ret_max){
			ret_max = end_cycles - midinv_cycles[cos_cpuid()];
		}

		if (end_cycles - midinv_cycles[cos_cpuid()] < ret_min || ret_min == 0){
			ret_min = end_cycles - midinv_cycles[cos_cpuid()];
		}
	}

	PRINTC("Test SINV:\t\t\t\t AVG:%lld, MAX:%lld, MIN:%lld, ITER:%lld\n", 
	        (total_inv_cycles / (long long)(ITER)), inv_max, inv_min, (long long) ITER);
	PRINTC("Test SRET:\t\t\t\t AVG:%lld, MAX:%lld, MIN:%lld, ITER:%lld\n", 
	        (total_ret_cycles / (long long)(ITER)), ret_max, ret_min, (long long) ITER);
//
//
//	PRINTC("Average SINV (Total: %lld / Iterations: %lld ): %lld\n", total_inv_cycles, (long long)(ITER),
//	       (total_inv_cycles / (long long)(ITER)));
//	PRINTC("Average SRET (Total: %lld / Iterations: %lld ): %lld\n", total_ret_cycles, (long long)(ITER),
//	       (total_ret_cycles / (long long)(ITER)));
}

void
test_print_ubench(void)
{
	PRINTC("Test Timer: \t\t\t\t AVG:%llu, MAX:%llu, MIN:%llu, ITER:%lld\n",
		    result.test_timer.avg, result.test_timer.max, result.test_timer.min, (long long) TEST_ITER);
	PRINTC("Test Single Budgets: \t\t\t AVG:%llu, MAX:%llu, MIN:%llu, ITER:%lld\n",
	        result.budgets_single.avg, result.budgets_single.max, result.budgets_single.min, (long long) TEST_ITER);

}

void
test_run_perf_mb(void)
{
	cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	test_async_endpoints_perf();
	test_inv_perf();
	test_print_ubench();

}
