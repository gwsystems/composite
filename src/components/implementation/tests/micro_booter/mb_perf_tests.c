#include <stdint.h>

#include "micro_booter.h"

extern int _expect_llu(int predicate, char *str, long long unsigned a, long long unsigned b, char *errcmp, char *testname, char *file, int line);
extern int _expect_ll(int predicate, char *str, long long a, long long b, char *errcmp, char *testname, char *file, int line);

unsigned int cyc_per_usec;
static volatile arcvcap_t rcc_global[NUM_CPU], rcp_global[NUM_CPU];
static volatile asndcap_t scc_global[NUM_CPU], scp_global[NUM_CPU];
static int                async_test_flag_[NUM_CPU] = { 0 };
volatile cycles_t		  main_thd = 0, side_thd = 0;

static void
spinner(void *d)
{
    while (1) {
		rdtscll(side_thd); 
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	}
}

static void
test_thds_create_switch(void)
{
    thdcap_t 			ts;
    int      			ret, i;
	long long unsigned 	t = 0, max = 0, min = 0;

    ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner, NULL);
    if (EXPECT_LL_LT(1, ts, "Thread Creation: Cannot Allocate")) {
        return;
    }

	for (i = 0; i < ITER; i++) {
		rdtscll(main_thd);
    	ret = cos_thd_switch(ts);
		EXPECT_LL_NEQ(0, ret, "COS Switch Error");

		t += side_thd - main_thd;

		if( side_thd - main_thd > max){
			max = side_thd - main_thd;
		}

		if( side_thd - main_thd < min || min == 0){
			min = side_thd - main_thd;
		}
	}

	PRINTC("\tCOS THD => COS_THD_SWITCH:\t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%lld\n",
		  (long long unsigned)(t / ITER), (long long unsigned) max, (long long unsigned) min, (long long) ITER);

	t = min = max = 0;

    for (i = 0; i < ITER; i++) {
		rdtscll(main_thd);
    	ret = cos_switch(ts, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, 0, 0, 0);
		EXPECT_LL_NEQ(0, ret, "COS Switch Error");

		t += side_thd - main_thd;

		if( side_thd - main_thd > max){
			max = side_thd - main_thd;
		}

		if( side_thd - main_thd < min || min == 0){
			min = side_thd - main_thd;
		}
	}

    PRINTC("\tCOS THD => COS_SWITCH:\t\t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%lld\n",
		  (long long unsigned)(t / ITER), (long long unsigned) max, (long long unsigned) min, (long long) ITER);
}

static void
async_thd_fn_perf(void *thdcap)
{
	thdcap_t  tc = (thdcap_t)thdcap;
	asndcap_t sc = scc_global[cos_cpuid()];
	arcvcap_t rc = rcc_global[cos_cpuid()];
	int       i, ret, pending = 0;

	for (i = 0; i < ITER; i++) {
		cos_rcv(rc, 0, NULL);
		cos_asnd(sc, 1);
	}

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
	arcvcap_t rc = rcc_global[cos_cpuid()];
	long long e	= 0, s = 0, t = 0, max = 0, min = 0;
	int       i, pending = 0;

	for (i = 0; i < ITER; i++) {
		rdtscll(s);
		cos_asnd(sc, 1);
		cos_rcv(rc, 0, NULL);
		rdtscll(e);

		t += e - s;

		if( e - s > max){
			max = e - s;
		}

		if( e - s < min || min == 0){
			min = e - s;
		}
	}

	PRINTC("\tAsync Endpoints => Roundtrip:\t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%lld\n",
		  (long long unsigned)(t / ITER), (long long unsigned) max, (long long unsigned) min, (long long) ITER);

	t = min = max = 0;

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

	PRINTC("\tAsync Endpoints => One Way:\t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%lld\n",
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

	/* make the snd channel to the parent */
	scc_global[cos_cpuid()] = cos_asnd_alloc(&booter_info, rcp, booter_info.captbl_cap);
	if(EXPECT_LL_EQ(0, scp_global[cos_cpuid()], "Test Async Endpoints")) return;

	rcc_global[cos_cpuid()] = rcc;
	rcp_global[cos_cpuid()] = rcp;

	async_test_flag_[cos_cpuid()] = 1;
	while (async_test_flag_[cos_cpuid()]) cos_thd_switch(tcp);
}

void
test_print_ubench(void)
{
	PRINTC("\tSINV:\t\t\t\t\tAVG:%lld, MAX:%lld, MIN:%lld, ITER:%lld\n", 
	        result.sinv.avg, result.sinv.max, result.sinv.min, (long long) ITER);
	PRINTC("\tSRET:\t\t\t\t\tAVG:%lld, MAX:%lld, MIN:%lld, ITER:%lld\n", 
	        result.sret.avg, result.sinv.max, result.sinv.min, (long long) ITER);
	PRINTC("\tTimer => Timeout Overhead: \t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%lld\n",
		    result.test_timer.avg, result.test_timer.max, result.test_timer.min, (long long) TEST_ITER);
	PRINTC("\tTimer => Budget based: \t\t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%lld\n",
	        result.budgets_single.avg, result.budgets_single.max, result.budgets_single.min, (long long) TEST_ITER);

}

void
test_run_perf_mb(void)
{
	cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	test_thds_create_switch();
	test_async_endpoints_perf();
	test_print_ubench();

}