#include <omp.h>
#include <stdio.h>

#include <cos_component.h>
#include <timed_blk.h>
#include <print.h>

//#define DISABLE

/* //cos specific  */
#include <cos_alloc.h>
#include <cos_synchronization.h>
#include <parlib.h>
#include <mem_mgr_large.h>

#define READER_CORE 0

#define printf printc

unsigned long long tsc_start(void)
{
	unsigned long cycles_high, cycles_low; 
	asm volatile ("mov $0, %%eax\n\t"
		      "CPUID\n\t"
		      "RDTSC\n\t"
		      "mov %%edx, %0\n\t"
		      "mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low) :: 
		      "%eax", "%ebx", "%ecx", "%edx");

	return ((unsigned long long)cycles_high << 32) | cycles_low;
}

unsigned long long tsc_end(void)
{
	/* This RDTSCP doesn't prevent memory re-ordering!. */
	unsigned long cycles_high1, cycles_low1; 
	asm volatile("RDTSCP\n\t"
		     "mov %%edx, %0\n\t"
		     "mov %%eax, %1\n\t"
		     "mov $0, %%eax\n\t"
		     "CPUID\n\t": "=r" (cycles_high1), "=r" (cycles_low1):: 
		     "%eax", "%ebx", "%ecx", "%edx");

	return ((unsigned long long)cycles_high1 << 32) | cycles_low1;
}

unsigned long long rdtsc(void)
{
	unsigned long cycles_high, cycles_low; 

	asm volatile ("RDTSCP\n\t" 
		      "mov %%edx, %0\n\t" 
		      "mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low) : : "%eax", "%edx"); 

	return ((unsigned long long)cycles_high << 32 | cycles_low);
}

#define ITER (100*1000*1000)

int *detector = (int *)0x4c3f0000;

#define CACHELINE_SIZE 64

int delay(int us) {
	unsigned long long s,e;
	volatile int mem = 0;

	s = rdtsc();
	while (1) {
		e = rdtsc();
		if (e - s > CPU_GHZ*1000*us) return 0; // x us
		mem++;
	}

	return 0;
}

struct thd_active {
	int accessed;
	int done;
	int avg;
	int max;
} CACHE_ALIGNED;

struct thd_active thd_active[NUM_CPU] CACHE_ALIGNED;

unsigned int reader_view[NUM_CPU_COS];

int cpu_assign[40] = {0, 4, 8, 12, 16, 20, 24, 28, 32, 36,
		      1, 5, 9, 13, 17, 21, 25, 29, 33, 37,
		      2, 6, 10, 14, 18, 22, 26, 30, 34, 38,
		      3, 7, 11, 15, 19, 23, 27, 31, 35, -1};

volatile int n_cores;
volatile int rate_gap;

volatile int not_likely = 0;

void meas_sync_start(void) {
	int cpu = cos_cpuid();
	ck_pr_store_int(&thd_active[cpu].done, 0);
	ck_pr_store_int(&thd_active[cpu].avg, 0);
	ck_pr_store_int(&thd_active[cpu].max, 0);

	if (cpu == 0) {
		int k = 1;
		while (k < n_cores) {
			while (1) {
				if (ck_pr_load_int(&thd_active[cpu_assign[k]].accessed)) break;
			}
			k++;
		}
		ck_pr_store_int(&thd_active[0].accessed, 1);
	} else {
		ck_pr_store_int(&thd_active[cpu].accessed, 1);
		while (ck_pr_load_int(&thd_active[0].accessed) == 0) ;
	} // sync!
}

void meas_sync_end() {
	int i;
	int cpu = cos_cpuid();
	ck_pr_store_int(&thd_active[cpu].accessed, 0);

	if (cpu == 0) { // output!!!
//		printf("test done %d, syncing\n", NUM_CPU_COS);
		// sync first!
		for (i = 1; i < n_cores;i++) {
			while (1) {
				if (ck_pr_load_int(&thd_active[cpu_assign[i]].done)) break;
			}
		}
		ck_pr_store_int(&thd_active[0].done, 1);
	} else {
		ck_pr_store_int(&thd_active[cpu].done, 1);

		while (ck_pr_load_int(&thd_active[0].done) == 0) ;
	}
}

__attribute__((packed)) struct shared_cacheline {
	int mem;
	char _pad[CACHELINE_SIZE - sizeof(int)];
} CACHE_ALIGNED;

struct shared_cacheline shared_mem;

static inline int null_op(int cpu, unsigned long long tsc) {
	return 0;
}

static inline int meas_faa_reader(int cpu, unsigned long long tsc) {
	volatile unsigned int stk_mem;

	stk_mem = ck_pr_load_int(&shared_mem.mem);

	assert(stk_mem != tsc);

	// op done!
	/* if (unlikely((unsigned int)stk_mem > tsc)) { */
	/* 	not_likely++;	 */
	/* 	return -1; */
	/* } */

	return 0;
}

static inline int meas_faa(int cpu, unsigned long long tsc) {
	volatile unsigned int stk_mem;

	stk_mem = ck_pr_faa_int(&shared_mem.mem, 1);

	// op done!
	if (unlikely((unsigned int)stk_mem > tsc)) {
		not_likely++;
		return -1;
	}

	return 0;
}

static inline int meas_cas_reader(int cpu, unsigned long long tsc) {
	volatile int stk_mem;

	stk_mem = ck_pr_load_int(&shared_mem.mem);

	// op done!
	assert(stk_mem < NUM_CPU_COS);
//	reader_view[stk_mem]++;
	/* if (unlikely((unsigned int)stk_mem > tsc)) { */
	/* 	not_likely++; */
	/* 	return -1; */
	/* } */

	return 0;
}

static inline int meas_cas(int cpu, unsigned long long tsc) {
	volatile int stk_mem;

	stk_mem = shared_mem.mem;
	ck_pr_cas_int(&shared_mem.mem, (int)stk_mem, (int) cpu);

	// op done!
	assert(stk_mem < NUM_CPU_COS);
	if (unlikely((unsigned int)stk_mem > tsc)) {
		not_likely++;
		return -1;
	}

	return 0;
}

static inline int meas_store_reader(int cpu, unsigned long long tsc) {
	volatile int stk_mem;

	stk_mem = ck_pr_load_int(&shared_mem.mem);

	// op done!
	assert(stk_mem < NUM_CPU_COS);
//	reader_view[stk_mem]++;
	/* if (unlikely((unsigned int)stk_mem > tsc)) { */
	/* 	not_likely++; */
	/* 	return -1; */
	/* } */

	return 0;
}

static inline int meas_store(int cpu, unsigned long long tsc) {
	volatile int stk_mem = 0;

	ck_pr_store_int(&shared_mem.mem, cpu);

	// op done!
	assert(stk_mem < NUM_CPU_COS);
	if (unlikely((unsigned int)stk_mem > tsc)) {
		not_likely++;
		return -1;
	}

	return 0;
}

static inline int meas_op(int (*op)(int cpu, unsigned long long tsc), char *name, unsigned long long gap) {
	//every core calls here!
	volatile int timer_if;
	unsigned long long s0, e0 = 0;
	int ii, k;
	int cpu = cos_cpuid();
	if (cpu == 0) {
		ck_pr_store_int(&shared_mem.mem, 0);
//		printc("%s starting\n", name);
	}

	meas_sync_start();

	unsigned long long sum = 0, max = 0, sum2 = 0, find_max = 0;
//	volatile unsigned long long tsc[16];
	unsigned long long s,e;
	int if_detected = 0, ret;
	unsigned long cycles_high, cycles_low, cycles_high1, cycles_low1, cycles_high2, cycles_low2;
	volatile int mem = 0;

	s0 = tsc_start();

	for (ii = 0; ii < ITER; ii++) {
		s = tsc_start();
		/* benchmark here! */
		op(cpu, s);
		/* benchmark done! */
		e = tsc_start();

		/* next the timer hack! */
		timer_if = ck_pr_load_int(&(detector[cpu*16]));
		if (unlikely(timer_if == 1)) {
			ii--;
			if_detected++;
			ck_pr_store_int(&(detector[cpu*16]), 0);
			find_max = 0;
			delay(100);
			continue;
		}

		find_max = e - s;
		assert(sum + find_max > sum);
		sum += find_max;
		if (find_max > max) max = find_max;

		/* Rate limit here. */
		e0 = tsc_start();
		mem = 0;
		while (e0 - e < (unsigned int)gap) {
			mem++;
			e0 = tsc_start();
		}
		assert(mem >= 0);
	}
	sum2 = e0 - s0;

	printc("%s cpu %ld (ticks tot %d,%d,%d): avg %llu max %llu\n",
	       name, cos_cpuid(), if_detected, (int)((sum2-gap*ITER)/(CPU_GHZ*10*1000000)), (int)(sum/(CPU_GHZ*10*1000000)), sum/ITER, max);
	
	/* printc("%s cpu %ld (sum %llu, sum2 %llu, ticks %d,%d): avg %llu max %llu\n", */
	/*        name, cos_cpuid(), sum, sum2, (int)((sum2-gap*ITER)/(CPU_GHZ*10*1000000)), (int)(sum/(CPU_GHZ*10*1000000)), sum/ITER, max); */

	if (((sum2 - sum)/ITER - gap) > 2000) {
		printc("\n\n\n\n !!!!!!!!!!!!%s cpu %ld ------------------- per op overhead %llu\n\n\n\n\n", 
		       name, cos_cpuid(), (sum2-sum)/ITER - gap);
//		BUG();
	}
	/* if ((int)((sum2-gap*ITER)/(CPU_GHZ*10*1000000)) - (int)(sum/(CPU_GHZ*10*1000000)) > 100) { */
	/* 	printc("\n\n\n\n check ticks!!!!!!!%s cpu %ld ------------------- per op overhead %llu\n\n\n\n\n",  */
	/* 	       name, cos_cpuid(), (sum2-sum)/ITER - gap); */
	/* } */

	ck_pr_store_int(&(thd_active[cos_cpuid()].avg), (int)(sum/ITER));
	ck_pr_store_int(&(thd_active[cos_cpuid()].max), (int)max);

	meas_sync_end();

	if (cpu == 0) {
		int i;
		int avg, max = 0, curr_max, cnt = 0;
		unsigned long long sum = 0;
		/* if (op == meas_cas || op == meas_store) { */
		/* 	printc("reader view...\n"); */
		/* 	for (i=0;i < NUM_CPU_COS;i++) { */
		/* 		printc("core %d: %u\n", i, reader_view[i]); */
		/* 		reader_view[i] = 0; */
		/* 	} */
		/* } */
		/* printc("%s finished!\n", name); */
		int reader_avg, reader_max;
		
		for (i = 0; i < NUM_CPU_COS; i++) {
			if (i == READER_CORE) {
				reader_avg = ck_pr_load_int(&(thd_active[i].avg));
				reader_max = ck_pr_load_int(&(thd_active[i].max));
			} else {
				avg = ck_pr_load_int(&(thd_active[i].avg));
				if (avg) {
					cnt++;
					sum += avg;
					curr_max = ck_pr_load_int(&(thd_active[i].max));
					if (curr_max > max) 
						max = curr_max;
				}
			}
			ck_pr_store_int(&(thd_active[i].avg), 0);
			ck_pr_store_int(&(thd_active[i].max), 0);
		}
		if (cnt) {
			avg = sum / cnt;
			printc(">>>>>>>>>>>>>>>>%s sum ncpu %d: avg %d max %d; reader avg %d, max %d\n", 
			       name, cnt, avg, max, reader_avg, reader_max);
		} else {
			printc(">>>>>>>>>>>>>>>>%s sum ncpu %d: reader avg %d, max %d\n", 
			       name, cnt, reader_avg, reader_max);
		}		
	}

	return 0;
}

static inline void go_par(int ncores) {
	int j;

	printc("Parallel benchmark: measuring %d cores, rate_gap %d\n", ncores, rate_gap);

	if (rate_gap == 0) {
#pragma omp parallel for
		for (j = 0; j < ncores; j++)
		{
			// per core below!
			assert(j == omp_get_thread_num());
			meas_op(null_op, "rdtsc_cost", rate_gap);
		}
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());

		if (cos_cpuid() == READER_CORE) meas_op(meas_faa_reader, "faa", rate_gap);
		else                            meas_op(meas_faa, "faa", rate_gap);
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		if (cos_cpuid() == READER_CORE) meas_op(meas_cas_reader, "cas", rate_gap);
		else                            meas_op(meas_cas, "cas", rate_gap);
	}

#pragma omp parallel for
	for (j = 0; j < ncores; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		if (cos_cpuid() == READER_CORE) meas_op(meas_store_reader, "mem store", rate_gap);
		else                            meas_op(meas_store, "mem store", rate_gap);
	}

	printc("Parallel benchmark: %d cores done\n", ncores);

	return;
}

int meas(void)
{
	int i, j, omp_cores;
	int gap = 0;
	
	printc("Parallel benchmark in component %ld\n", cos_spd_id());
	mman_alias_page(cos_spd_id(), 10, 10, 9999, MAPPING_RW);

#pragma omp parallel for
	for (i = 0; i < NUM_CPU_COS; i++) 
		if (i == 0) omp_cores = omp_get_num_threads();

	int rates[10] = {0, 1, 5, 10, 20, 30, 50};
	
//	for (gap = 0; gap <= (CPU_GHZ * 100 * 1000); gap += (10 * CPU_GHZ * 1000)) {
	for (i = 1; i < 3; i++) {
//	{
//		i = 4;
		gap = rates[i] * (CPU_GHZ*1000);
		rate_gap = gap;

		/* n_cores = omp_cores; */
		/* go_par(n_cores); */

		n_cores = 10;
		go_par(n_cores);
	}

	rate_gap = 0;
	n_cores = 1;
	go_par(n_cores);

	if (not_likely)
		printc("\n\n\n\n!!!!!!!!!!!!!!not likely events: %d\n\n\n\n\n\n", not_likely);

	return 0;
}

int main() {
#ifdef DISABLE 
	return 0;
#endif
	if (NUM_CPU_COS == 1) {
		printc("Par test but Composite only has 1 cpu. No parallel execution can be done.\n"
		       "NUM_CPU needs to be greater than 2 to enable parallel execution in Composite.\n");
	}
	union sched_param sp;
	
	sp.c.type  = SCHEDP_PRIO;
	sp.c.value = 10;

	cos_thd_create(meas, NULL, sp.v, 0, 0);

	return 0;
}
