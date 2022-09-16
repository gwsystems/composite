/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_defkernel_api.h>
#include <llprint.h>
#include <res_spec.h>
#include <sl.h>

#define FIXED_PRIORITY 10

static int thd1_reg[NUM_CPU] = { 0 };
static int thd2_fpu[NUM_CPU] = { 0 };
static int thd3_fpu[NUM_CPU] = { 0 };

/* TODO: make this test generic for different architectures */
#define YMM_N_REGS 16
#define YMM_SIZE 4

thdid_t g_thd1 = 0;
u64_t fpu_thd1_load[YMM_N_REGS][YMM_SIZE];
u64_t fpu_thd1_read[YMM_N_REGS][YMM_SIZE];

thdid_t g_thd2 = 0;
u64_t fpu_thd2_load[YMM_N_REGS][YMM_SIZE];
u64_t fpu_thd2_read[YMM_N_REGS][YMM_SIZE];

static volatile int g_thd1_done = 0;
static volatile int g_thd2_done = 0;

static void
fpu1_thd_fn()
{
	int i, j, round = 3000;
	u64_t rand;
	
	g_thd1 = cos_thdid();

	while (--round) {
		/* set a rand number for each ymm register */
		for (i = 0; i < YMM_N_REGS; i++) {
			for (j = 0; j < YMM_SIZE; j++) {
				rdtscll(rand);
				fpu_thd1_load[i][j] = rand << 32 | rand;;
			}
		}

		/* Load all ymm registers */
		__asm__ __volatile__ ("vmovups %0, %%ymm0\n\t"::"m"(fpu_thd1_load[0]));
		__asm__ __volatile__ ("vmovups %0, %%ymm1\n\t"::"m"(fpu_thd1_load[1]));
		__asm__ __volatile__ ("vmovups %0, %%ymm2\n\t"::"m"(fpu_thd1_load[2]));
		__asm__ __volatile__ ("vmovups %0, %%ymm3\n\t"::"m"(fpu_thd1_load[3]));
		__asm__ __volatile__ ("vmovups %0, %%ymm4\n\t"::"m"(fpu_thd1_load[4]));
		__asm__ __volatile__ ("vmovups %0, %%ymm5\n\t"::"m"(fpu_thd1_load[5]));
		__asm__ __volatile__ ("vmovups %0, %%ymm6\n\t"::"m"(fpu_thd1_load[6]));
		__asm__ __volatile__ ("vmovups %0, %%ymm7\n\t"::"m"(fpu_thd1_load[7]));
		__asm__ __volatile__ ("vmovups %0, %%ymm8\n\t"::"m"(fpu_thd1_load[8]));
		__asm__ __volatile__ ("vmovups %0, %%ymm9\n\t"::"m"(fpu_thd1_load[9]));
		__asm__ __volatile__ ("vmovups %0, %%ymm10\n\t"::"m"(fpu_thd1_load[10]));
		__asm__ __volatile__ ("vmovups %0, %%ymm11\n\t"::"m"(fpu_thd1_load[11]));
		__asm__ __volatile__ ("vmovups %0, %%ymm12\n\t"::"m"(fpu_thd1_load[12]));
		__asm__ __volatile__ ("vmovups %0, %%ymm13\n\t"::"m"(fpu_thd1_load[13]));
		__asm__ __volatile__ ("vmovups %0, %%ymm14\n\t"::"m"(fpu_thd1_load[14]));
		__asm__ __volatile__ ("vmovups %0, %%ymm15\n\t"::"m"(fpu_thd1_load[15]));

		/* Force thread switch */
		sl_thd_block(0);
		assert(g_thd2 != 0);
		sl_thd_wakeup(g_thd2);

		/* Read them back */
		__asm__ __volatile__ ("vmovups %%ymm0, %0\n\t":"=m"(fpu_thd1_read[0]));
		__asm__ __volatile__ ("vmovups %%ymm1, %0\n\t":"=m"(fpu_thd1_read[1]));
		__asm__ __volatile__ ("vmovups %%ymm2, %0\n\t":"=m"(fpu_thd1_read[2]));
		__asm__ __volatile__ ("vmovups %%ymm3, %0\n\t":"=m"(fpu_thd1_read[3]));
		__asm__ __volatile__ ("vmovups %%ymm4, %0\n\t":"=m"(fpu_thd1_read[4]));
		__asm__ __volatile__ ("vmovups %%ymm5, %0\n\t":"=m"(fpu_thd1_read[5]));
		__asm__ __volatile__ ("vmovups %%ymm6, %0\n\t":"=m"(fpu_thd1_read[6]));
		__asm__ __volatile__ ("vmovups %%ymm7, %0\n\t":"=m"(fpu_thd1_read[7]));
		__asm__ __volatile__ ("vmovups %%ymm8, %0\n\t":"=m"(fpu_thd1_read[8]));
		__asm__ __volatile__ ("vmovups %%ymm9, %0\n\t":"=m"(fpu_thd1_read[9]));
		__asm__ __volatile__ ("vmovups %%ymm10, %0\n\t":"=m"(fpu_thd1_read[10]));
		__asm__ __volatile__ ("vmovups %%ymm11, %0\n\t":"=m"(fpu_thd1_read[11]));
		__asm__ __volatile__ ("vmovups %%ymm12, %0\n\t":"=m"(fpu_thd1_read[12]));
		__asm__ __volatile__ ("vmovups %%ymm13, %0\n\t":"=m"(fpu_thd1_read[13]));
		__asm__ __volatile__ ("vmovups %%ymm14, %0\n\t":"=m"(fpu_thd1_read[14]));
		__asm__ __volatile__ ("vmovups %%ymm15, %0\n\t":"=m"(fpu_thd1_read[15]));

		/* Make sure the ymm registers of this thread is not corrupted */
		for (i = 0; i < YMM_N_REGS; i++) {
			for (j = 0; j < YMM_SIZE; j++) {
				assert(fpu_thd1_load[i][j] == fpu_thd1_read[i][j]);
			}
		}
	}
	
	g_thd1_done = 1;
	sl_thd_exit();
}

static void
fpu2_thd_fn()
{
	int i, j, round = 3000;
	u64_t rand;

	g_thd2 = cos_thdid();

	while (--round) {
		/* set a rand number for each ymm register */
		for (i = 0; i < YMM_N_REGS; i++) {
			for (j = 0; j < YMM_SIZE; j++) {
				rdtscll(rand);
				fpu_thd2_load[i][j] = rand << 32 | rand;
			}
		}

		/* Load all ymm registers */
		__asm__ __volatile__ ("vmovups %0, %%ymm0\n\t"::"m"(fpu_thd2_load[0]));
		__asm__ __volatile__ ("vmovups %0, %%ymm1\n\t"::"m"(fpu_thd2_load[1]));
		__asm__ __volatile__ ("vmovups %0, %%ymm2\n\t"::"m"(fpu_thd2_load[2]));
		__asm__ __volatile__ ("vmovups %0, %%ymm3\n\t"::"m"(fpu_thd2_load[3]));
		__asm__ __volatile__ ("vmovups %0, %%ymm4\n\t"::"m"(fpu_thd2_load[4]));
		__asm__ __volatile__ ("vmovups %0, %%ymm5\n\t"::"m"(fpu_thd2_load[5]));
		__asm__ __volatile__ ("vmovups %0, %%ymm6\n\t"::"m"(fpu_thd2_load[6]));
		__asm__ __volatile__ ("vmovups %0, %%ymm7\n\t"::"m"(fpu_thd2_load[7]));
		__asm__ __volatile__ ("vmovups %0, %%ymm8\n\t"::"m"(fpu_thd2_load[8]));
		__asm__ __volatile__ ("vmovups %0, %%ymm9\n\t"::"m"(fpu_thd2_load[9]));
		__asm__ __volatile__ ("vmovups %0, %%ymm10\n\t"::"m"(fpu_thd2_load[10]));
		__asm__ __volatile__ ("vmovups %0, %%ymm11\n\t"::"m"(fpu_thd2_load[11]));
		__asm__ __volatile__ ("vmovups %0, %%ymm12\n\t"::"m"(fpu_thd2_load[12]));
		__asm__ __volatile__ ("vmovups %0, %%ymm13\n\t"::"m"(fpu_thd2_load[13]));
		__asm__ __volatile__ ("vmovups %0, %%ymm14\n\t"::"m"(fpu_thd2_load[14]));
		__asm__ __volatile__ ("vmovups %0, %%ymm15\n\t"::"m"(fpu_thd2_load[15]));

		/* Force thread switch */
		assert(g_thd1 != 0);
		sl_thd_wakeup(g_thd1);
		sl_thd_block(0);

		/* Read them back */
		__asm__ __volatile__ ("vmovups %%ymm0, %0\n\t":"=m"(fpu_thd2_read[0]));
		__asm__ __volatile__ ("vmovups %%ymm1, %0\n\t":"=m"(fpu_thd2_read[1]));
		__asm__ __volatile__ ("vmovups %%ymm2, %0\n\t":"=m"(fpu_thd2_read[2]));
		__asm__ __volatile__ ("vmovups %%ymm3, %0\n\t":"=m"(fpu_thd2_read[3]));
		__asm__ __volatile__ ("vmovups %%ymm4, %0\n\t":"=m"(fpu_thd2_read[4]));
		__asm__ __volatile__ ("vmovups %%ymm5, %0\n\t":"=m"(fpu_thd2_read[5]));
		__asm__ __volatile__ ("vmovups %%ymm6, %0\n\t":"=m"(fpu_thd2_read[6]));
		__asm__ __volatile__ ("vmovups %%ymm7, %0\n\t":"=m"(fpu_thd2_read[7]));
		__asm__ __volatile__ ("vmovups %%ymm8, %0\n\t":"=m"(fpu_thd2_read[8]));
		__asm__ __volatile__ ("vmovups %%ymm9, %0\n\t":"=m"(fpu_thd2_read[9]));
		__asm__ __volatile__ ("vmovups %%ymm10, %0\n\t":"=m"(fpu_thd2_read[10]));
		__asm__ __volatile__ ("vmovups %%ymm11, %0\n\t":"=m"(fpu_thd2_read[11]));
		__asm__ __volatile__ ("vmovups %%ymm12, %0\n\t":"=m"(fpu_thd2_read[12]));
		__asm__ __volatile__ ("vmovups %%ymm13, %0\n\t":"=m"(fpu_thd2_read[13]));
		__asm__ __volatile__ ("vmovups %%ymm14, %0\n\t":"=m"(fpu_thd2_read[14]));
		__asm__ __volatile__ ("vmovups %%ymm15, %0\n\t":"=m"(fpu_thd2_read[15]));

		/* Make sure the ymm registers of this thread is not corrupted */
		for (i = 0; i < YMM_N_REGS; i++) {
			for (j = 0; j < YMM_SIZE; j++) {
				assert(fpu_thd2_load[i][j] == fpu_thd2_read[i][j]);
			}
		}
	}
	
	g_thd2_done = 1;
	sl_thd_exit();
}

static void
reg_thd_fn()
{
	thd1_reg[cos_cpuid()] = 1;
	g_thd1_done = 1;
	while (1);
}

static void
pi_thd_fn()
{
	float    PI    = 3.0;
	int      flag  = 1;
	int      i;

	thd2_fpu[cos_cpuid()] = 1;
	for (i = 2; i < 100000; i += 2) {	
		if (flag) {
			PI += (4.0 / (i * (i + 1) * (i + 2)));
		} else {
			PI -= (4.0 / (i * (i + 1) * (i + 2)));
		}
		flag = !flag;
	}
	// PRINTC("\tpi = %f: \t\t\tFinish calculate Pi\n", PI);
	g_thd2_done = 1;
	sl_thd_exit();
}

static void
euler_thd_fn()
{
	float    E    = 1.0;
	float    fact = 1.0;
	int      i;

	thd3_fpu[cos_cpuid()] = 1;
	for (i = 1; i < 1000; i++) {	
		fact *= i;
		E += (1.0 / fact);
	}
	g_thd1_done = 1;
	// PRINTC("\te = %f: \t\t\tFinish calculate E\n", E);
	sl_thd_exit();
}

static void
allocator_ff_thread_fn()
{
	struct sl_thd *thd1, *thd2;
	cycles_t wakeup;

	thd1 = sl_thd_alloc(pi_thd_fn, NULL);
	sl_thd_param_set(thd1, sched_param_pack(SCHEDP_PRIO, FIXED_PRIORITY));

	thd2 = sl_thd_alloc(euler_thd_fn, NULL);
	sl_thd_param_set(thd2, sched_param_pack(SCHEDP_PRIO, FIXED_PRIORITY));

	while (!g_thd1_done || !g_thd2_done);

	sl_thd_free(thd1);
	sl_thd_free(thd2);

	sl_thd_exit();
}

static void
allocator_fr_thread_fn()
{
	struct sl_thd *thd1, *thd2;
	cycles_t wakeup;

	thd1 = sl_thd_alloc(reg_thd_fn, NULL);
	sl_thd_param_set(thd1, sched_param_pack(SCHEDP_PRIO, FIXED_PRIORITY));

	thd2 = sl_thd_alloc(pi_thd_fn, NULL);
	sl_thd_param_set(thd2, sched_param_pack(SCHEDP_PRIO, FIXED_PRIORITY));

	while (!g_thd1_done || !g_thd2_done);

	sl_thd_free(thd1);
	sl_thd_free(thd2);

	sl_thd_exit();
}

static void
allocator_fpu_thread_fn()
{
	struct sl_thd *thd1, *thd2;
	cycles_t wakeup;

	thd1 = sl_thd_alloc(fpu1_thd_fn, NULL);
	sl_thd_param_set(thd1, sched_param_pack(SCHEDP_PRIO, FIXED_PRIORITY));


	thd2 = sl_thd_alloc(fpu2_thd_fn, NULL);
	sl_thd_param_set(thd2, sched_param_pack(SCHEDP_PRIO, FIXED_PRIORITY));

	while (!g_thd1_done || !g_thd2_done);

	sl_thd_free(thd1);
	sl_thd_free(thd2);

	sl_thd_exit();
}

static void
test_swapping(int d)
{
	struct sl_thd *allocator_thread;
	cycles_t wakeup;

	g_thd1_done = 0;
	g_thd2_done = 0;

	if (d == 0) {
		allocator_thread = sl_thd_alloc(allocator_ff_thread_fn, NULL);
	} else if (d == 1) {
		allocator_thread = sl_thd_alloc(allocator_fr_thread_fn, NULL);
	} else if (d == 2) {
		allocator_thread = sl_thd_alloc(allocator_fpu_thread_fn, NULL);
	}
	sl_thd_param_set(allocator_thread, sched_param_pack(SCHEDP_PRIO, FIXED_PRIORITY));

	while (!g_thd1_done || !g_thd2_done);

	sl_thd_free(allocator_thread);
}

static void
run_tests()
{
	test_swapping(0);
	PRINTC("Test set1, result: SUCCESS!\n");

	test_swapping(1);
	PRINTC("Test set2, result: SUCCESS!\n");

	test_swapping(2);
	PRINTC("Test set3, result: SUCCESS!\n");

	PRINTC("Unit-test done!\n");
	sl_thd_exit();
}

void
cos_init(void)
{
	int i;
	static unsigned long first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };
	struct sl_thd *testing_thread;
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	PRINTC("Unit-test for the fpu\n");

	if (ps_cas(&first, NUM_CPU + 1, cos_cpuid())) {
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();
	} else {
		while (!ps_load(&init_done[first])) ;

		cos_defcompinfo_sched_init();
	}
	ps_faa(&init_done[cos_cpuid()], 1);
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load(&init_done[i])) ;
	}

	sl_init(SL_MIN_PERIOD_US);

	testing_thread = sl_thd_alloc(run_tests, NULL);
	sl_thd_param_set(testing_thread, sched_param_pack(SCHEDP_PRIO, FIXED_PRIORITY));

	sl_sched_loop();

	assert(0);

	return;
}

