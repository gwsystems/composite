#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

int
prints(char *s)
{
    int len = strlen(s);
	  cos_print(s, len);
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
	  cos_print(s, ret);

	  return ret;
}

#ifndef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); cos_thd_switch();} } while(0)
#endif

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

struct cos_compinfo booter_info;

#define ITERATIONS 1000000
#define TEST_NTHDS 5
int tls_test[TEST_NTHDS];

static unsigned long
tls_get(size_t off)
{
	unsigned long val;

	__asm__ __volatile__("movl %%gs:(%1), %0" : "=r" (val) : "r" (off) : );

	return val;
}

static void
tls_set(size_t off, unsigned long val)
{ __asm__ __volatile__("movl %0, %%gs:(%1)" : : "r" (val), "r" (off) : "memory"); }

static void
thd_fn_perf(void *d)
{
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);

	while(1) {
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
	printc("Error, shouldn't get here!\n");
}

static void
test_thds_perf(void)
{
	thdcap_t ts;
	int i = 0;
	long long total_swt_cycles = 0LL;
	long long start_swt_cycles = 0LL, end_swt_cycles = 0LL;
	long long iters = 0LL;

	ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn_perf, (void *)i);
	assert(ts);
	cos_thd_switch(ts);

	rdtscll(start_swt_cycles);
	for (iters = 0LL; iters < ITERATIONS; iters ++) {
		cos_thd_switch(ts);
	}
	rdtscll(end_swt_cycles);
	total_swt_cycles = (end_swt_cycles - start_swt_cycles) / 2LL;

	printc("Average THD SWTCH (Total: %lld / Iterations: %lld ): %lld\n", 
		total_swt_cycles, (long long) ITERATIONS, (total_swt_cycles / (long long)ITERATIONS));
}

static void
thd_fn(void *d)
{
	printc("\tNew thread %d with argument %d, capid %d\n", cos_thdid(), (int)d, tls_test[(int)d]);
	/* Test the TLS support! */
	assert(tls_get(0) == tls_test[(int)d]);
	while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	printc("Error, shouldn't get here!\n");
}

static void
test_thds(void)
{
	thdcap_t ts[TEST_NTHDS];
	int i;

	for (i = 0 ; i < TEST_NTHDS ; i++) {
		ts[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn, (void *)i);
		assert(ts[i]);
		tls_test[i] = i;
		cos_thd_mod(&booter_info, ts[i], &tls_test[i]);
		printc("switchto %d\n", (int)ts[i]);
		cos_thd_switch(ts[i]);
	}

	printc("test done\n");
}

static void
test_mem(void)
{
	char *p = cos_page_bump_alloc(&booter_info);

	assert(p);
	strcpy(p, "victory");

	printc("Page allocation: %s\n", p);
}

volatile arcvcap_t rcc_global, rcp_global;
volatile asndcap_t scp_global;
int async_test_flag = 0;

static void
async_thd_fn_perf(void *thdcap)
{
	thdcap_t tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcc_global;
	thdid_t tid;
	int rcving;
	cycles_t cycles;
	long long iters = 0LL;

	cos_rcv(rc, &tid, &rcving, &cycles);

	for (iters = 0LL; iters < ITERATIONS + 1; iters ++) {
		cos_rcv(rc, &tid, &rcving, &cycles);
	}

	cos_thd_switch(tc);
}

static void
async_thd_parent_perf(void *thdcap)
{
	thdcap_t tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcp_global;
	asndcap_t sc = scp_global;
	long long total_asnd_cycles = 0LL;
	long long start_asnd_cycles = 0LL, end_arcv_cycles = 0LL;
	long long iters = 0LL;

	cos_asnd(sc);

	rdtscll(start_asnd_cycles);
	for (iters = 0LL; iters < ITERATIONS; iters ++) {
		cos_asnd(sc);
	}
	rdtscll(end_arcv_cycles);
	total_asnd_cycles = (end_arcv_cycles - start_asnd_cycles) / 2LL ;

	printc("Average ASND/ARCV (Total: %lld / Iterations: %lld ): %lld\n", 
		total_asnd_cycles, (long long) (ITERATIONS), (total_asnd_cycles / (long long)(ITERATIONS)));

	async_test_flag = 0;
	cos_thd_switch(tc);
}

static void
async_thd_fn(void *thdcap)
{
	thdcap_t tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcc_global;
	thdid_t tid;
	int rcving;
	cycles_t cycles;
	int pending;

	printc("Asynchronous event thread handler.\n<-- rcving...\n");
	pending = cos_rcv(rc, &tid, &rcving, &cycles);
	printc("<-- pending %d, thdid %d, rcving %d, cycles %lld\n<-- rcving...\n", pending, tid, rcving, cycles);
	pending = cos_rcv(rc, &tid, &rcving, &cycles);
	printc("<-- pending %d, thdid %d, rcving %d, cycles %lld\n<-- rcving...\n", pending, tid, rcving, cycles);
	pending = cos_rcv(rc, &tid, &rcving, &cycles);
	printc("<-- Error: manually returning to snding thread.\n");
	cos_thd_switch(tc);
	printc("ERROR: in async thd *after* switching back to the snder.\n");
	while (1) ;
}

static void
async_thd_parent(void *thdcap)
{
	thdcap_t tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcp_global;
	asndcap_t sc = scp_global;
	int ret, pending;
	thdid_t tid;
	int rcving;
	cycles_t cycles;

	printc("--> sending\n");
	ret = cos_asnd(sc);
	if (ret) printc("asnd returned %d.\n", ret);
	printc("--> Back in the asnder.\n--> sending\n");
	ret = cos_asnd(sc);
	if (ret) printc("--> asnd returned %d.\n", ret);
	printc("--> Back in the asnder.\n--> receiving to get notifications\n");
	pending = cos_rcv(rc, &tid, &rcving, &cycles);
	printc("--> pending %d, thdid %d, rcving %d, cycles %lld\n", pending, tid, rcving, cycles);

	async_test_flag = 0;
	cos_thd_switch(tc);
}

static void
test_async_endpoints(void)
{
	thdcap_t tcp, tcc;
	arcvcap_t rcp, rcc;

	printc("Creating threads, and async end-points.\n");
	/* parent rcv capabilities */
	tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent, (void*)BOOT_CAPTBL_SELF_INITTHD_BASE);
	assert(tcp);
	rcp = cos_arcv_alloc(&booter_info, tcp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
	assert(rcp);

	/* child rcv capabilities */
	tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn, (void*)tcp);
	assert(tcc);
	rcc = cos_arcv_alloc(&booter_info, tcc, booter_info.comp_cap, rcp);
	assert(rcc);

	/* make the snd channel to the child */
	scp_global = cos_asnd_alloc(&booter_info, rcc, booter_info.captbl_cap);
	assert(scp_global);

	rcc_global = rcc;
	rcp_global = rcp;

	async_test_flag = 1;
	while (async_test_flag) cos_thd_switch(tcp);

	printc("Async end-point test successful.\nTest done.\n");
}

static void
test_async_endpoints_perf(void)
{
	thdcap_t tcp, tcc;
	arcvcap_t rcp, rcc;

	/* parent rcv capabilities */
	tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent_perf, (void*)BOOT_CAPTBL_SELF_INITTHD_BASE);
	assert(tcp);
	rcp = cos_arcv_alloc(&booter_info, tcp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
	assert(rcp);

	/* child rcv capabilities */
	tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn_perf, (void*)tcp);
	assert(tcc);
	rcc = cos_arcv_alloc(&booter_info, tcc, booter_info.comp_cap, rcp);
	assert(rcc);

	/* make the snd channel to the child */
	scp_global = cos_asnd_alloc(&booter_info, rcc, booter_info.captbl_cap);
	assert(scp_global);

	rcc_global = rcc;
	rcp_global = rcp;

	async_test_flag = 1;
	while (async_test_flag) cos_thd_switch(tcp);
}

static void
spinner(void *d)
{ while (1) ; }

static void
test_timer(void)
{
	int i;
	thdcap_t tc;

	printc("Starting timer test.\n");
	tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner, NULL);

	for (i = 0 ; i < 10 ; i++) {
		thdid_t tid;
		int rcving;
		cycles_t cycles;

		printc(".");
		cos_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &rcving, &cycles);
		cos_thd_switch(tc);
	}

	printc("Timer test completed.\nSuccess.\n");
}

long long midinv_cycles = 0LL;

int
test_serverfn(int a, int b, int c)
{
	rdtscll(midinv_cycles);
	return 0;
}

extern void *__inv_test_serverfn(int a, int b, int c);

static inline
int call_cap_mb(u32_t cap_no, int arg1, int arg2, int arg3)
{
	int ret;

	/*
	 * Which stack should we use for this invocation?  Simple, use
	 * this stack, at the current sp.  This is essentially a
	 * function call into another component, with odd calling
	 * conventions.
	 */
	cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;

	__asm__ __volatile__( \
		"pushl %%ebp\n\t" \
		"movl %%esp, %%ebp\n\t" \
		"movl %%esp, %%edx\n\t" \
		"movl $1f, %%ecx\n\t" \
		"sysenter\n\t" \
		"1:\n\t" \
		"popl %%ebp" \
		: "=a" (ret)
		: "a" (cap_no), "b" (arg1), "S" (arg2), "D" (arg3) \
		: "memory", "cc", "ecx", "edx");

	return ret;
}

static void
test_inv(void)
{
	compcap_t cc;
	sinvcap_t ic;
	unsigned int r;

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	assert(cc > 0);
	ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn);
	assert(ic > 0);

	r = call_cap_mb(ic, 1, 2, 3);
	printc("Return from invocation: %d\n", r);
	printc("Test done.\n");
}

static void
test_inv_perf(void)
{
	compcap_t cc;
	sinvcap_t ic;
	long long iters = 0LL;
	long long total_cycles = 0LL;
	long long total_inv_cycles = 0LL, total_ret_cycles = 0LL;

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	assert(cc > 0);
	ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn);
	assert(ic > 0);

	for (iters = 0LL; iters < ITERATIONS; iters ++) {
		long long start_cycles = 0LL, end_cycles = 0LL;

		midinv_cycles = 0LL;
		rdtscll(start_cycles);
		call_cap_mb(ic, 1, 2, 3);
		rdtscll(end_cycles);
		total_inv_cycles += (midinv_cycles - start_cycles);
		total_ret_cycles += (end_cycles - midinv_cycles);
	}

	printc("Average SINV (Total: %lld / Iterations: %lld ): %lld\n", 
		total_inv_cycles, (long long) (ITERATIONS), (total_inv_cycles / (long long)(ITERATIONS)));
	printc("Average SRET (Total: %lld / Iterations: %lld ): %lld\n", 
		total_ret_cycles, (long long) (ITERATIONS), (total_ret_cycles / (long long)(ITERATIONS)));
}

void
cos_init(void)
{
	printc("\nMicro Booter started.\n");

	cos_meminfo_init(&booter_info.mi, BOOT_MEM_PM_BASE, COS_MEM_USER_PA_SZ,
			 BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);

	printc("---------------------------\n");
	test_thds();
	printc("---------------------------\n");
	test_thds_perf();
	printc("---------------------------\n");

	printc("---------------------------\n");
	test_timer();
	printc("---------------------------\n");

	printc("---------------------------\n");
	test_mem();
	printc("---------------------------\n");

	printc("---------------------------\n");
	test_async_endpoints();
	printc("---------------------------\n");
	test_async_endpoints_perf();
	printc("---------------------------\n");

	printc("---------------------------\n");
	test_inv();
	printc("---------------------------\n");
	test_inv_perf();
	printc("---------------------------\n");

	printc("\nMicro Booter done.\n");

//	while (1) ;
	BUG();

	return;
}
