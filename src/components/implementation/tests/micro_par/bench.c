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

#define printf printc

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

#define ITER (1 * 1000)
unsigned int time0[ITER], time1[ITER];
int core_access[NUM_CPU_COS];

struct thd_active {
	volatile int accessed CACHE_ALIGNED;
};

#define ITER_1M (10*1000*1000)

volatile struct thd_active thd_active[NUM_CPU_COS] CACHE_ALIGNED;

int *detector = 0x4c3f0000;

int delay(int us) {
	unsigned long long s,e;
	volatile int mem = 0;

	rdtscll(s);
	while (1) {
		rdtscll(e);
		if (e - s > CPU_GHZ*1000*us) return 0; // x us
		mem++;
	}

	return 0;
}

int n_cores;

static int test(void) {
	volatile int timer_if;
	int i, k;
	unsigned long long s, e;

	unsigned long long sum = 0, max = 0, diff;
	for (i = 0; i < ITER_1M; i++) {
		timer_if = 0;

		rdtscll(s);
		rdtscll(e);

		/* for (k = 0; k < n_cores; k++) { */
		timer_if = ck_pr_load_int(&detector[cos_cpuid()*8]);
		/* if (detector[cos_cpuid()*8] > 0) { */
		/* 	timer_if = 1; */
		/* } */
		/* } */
		if (timer_if == 1) {
//			printc("timer detected @ %llu\n", s);
			i--;
			ck_pr_store_int(&detector[cos_cpuid()*8], 0);
			delay(100);
			continue;
		}

		if (e - s > 5000) {
			printc("BIG gap on core %d: %llu cycles\n",
			       cos_cpuid(), e - s);
		}
		diff = e-s;
		sum += diff;
		if (diff > max) max = diff;
	}
	printc("cpu %d: sum %llu, avg %llu, max %llu;\n", 
	       cos_cpuid(), sum, sum/ITER_1M, max);
	return 0;
}

int meas(void)
{
	volatile int a, b = 0;
	int i, j, my_id, loop;

	if (NUM_CPU_COS == 1) {
		printc("Par test but Composite only has 1 cpu. No parallel execution can be done.\n");
		printc("NUM_CPU needs to be greater than 2 to enable parallel execution in Composite.\n");
	}

	printc("bench comp %d\n", cos_spd_id());
	mman_alias_page(cos_spd_id(), 10, 10, 9999, MAPPING_RW);

	printc("testing fpu...\n");
	volatile float f;
	for (i = 10; i < 1000; i++) {
		f = i;
		f = f / 3;
		f = f * 3;
		assert((int)f == i);

		if (f > 10000) break;
			/*fpu test*/
	}
	if (f > 10000) return 0;
	printc("testing fpu done.\n");
#pragma omp parallel for
	for (i = 0; i < NUM_CPU_COS; i++) 
		if (i == 0) loop = omp_get_num_threads();

	n_cores = loop;

#pragma omp parallel for
	for (j = 0; j < loop; j++)
	{
		// per core below!
		assert(j == omp_get_thread_num());
		test();
	}

	/* for (i = 0; i < ITER; i+=4) { */
	/* 	//do this to save the dmesg buffer. time stamp is long! */
	/* 	printc("%u %u %u %u %u %u %u %u\n", */
	/* 	       time0[i], time1[i], time0[i+1], time1[i+1], time0[i+2], time1[i+2],time0[i+3], time1[i+3]); */
	/* } */


	unsigned long long sum = 0, max = 0, sum2 = 0, max2 = 0;
	for (i = 0; i < ITER; i++) {
		//printc("%u \n", time0[i], time1[i]);
		sum += time0[i];
		sum2 += time1[i];
		printc("%d:%u\n", i, time0[i]);
		if (time0[i] > max && i > 0) max = time0[i];
		if (time1[i] > max2 && i > 0) max2 = time1[i];
	}
	sum /= ITER;
	sum2 /= ITER;
	printc("ncpus: %d, avg %llu, max %llu; avg2 %llu, max2 %llu\n", loop, sum, max, sum2, max2);

	return 0;
}

int main() {
	/* int nthds; */
	/* int i,j,k; */
	/* volatile int my_id; */
#ifdef DISABLE 
	return 0;
#endif

	meas();
/* #pragma omp parallel private(my_id) */
/* 	{ */
		/* my_id = omp_get_thread_num(); */
		/* /\* printf("omp thread running: my_id is %d\n", my_id); *\/ */

		/* nthds = omp_get_num_threads(); */

	/* 	if (my_id == 0) */
	/* 		printf("main thread: total thds %d\n", nthds); */

	/* 	for (i = 0; i < 5000; i++) { */
	/* 		for (j = 0; j < 5000; j++) { */
	/* 			if (my_id != omp_get_thread_num()) { */
	/* 				printf("private variable test FAIL! thd %d, var %d\n", omp_get_thread_num(), my_id); */
	/* 				break; */
	/* 			} */
	/* 			mem++; */
	/* 		} */
	/* 	} */

	/* 	printf("thd %d (private var %d) spinning done!\n", omp_get_thread_num(), my_id); */
	/* } */

	/* volatile int mem = 1; */

// nested looping test below
/* #pragma omp parallel for private(j) */
/* 	for (i = 0; i < 2; i++) { */
/* 		omp_set_nested(1); */
/* 		printf("thd %d, %d thds, outer loop i %d\n",  */
/* 		       omp_get_thread_num(), omp_get_num_threads(), i); */
/* #pragma omp parallel for */
/* 		for (j = 0; j < 4; j++) { */
/* 			printf("thd %d, %d thds, inner loop i %d, j %d\n",  */
/* 			       omp_get_thread_num(), omp_get_num_threads(), i, j); */
/* 		} */
/* 		printf("thd %d, %d thds, outer loop end i %d\n",  */
/* 		       omp_get_thread_num(), omp_get_num_threads(), i); */
/* 	} */

	return 0;
}
