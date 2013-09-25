#include <omp.h>
#include <stdio.h>

#include <cos_component.h>
#include <timed_blk.h>
#include <print.h>

//#define DISABLE

/* //cos specific  */
#include <cos_alloc.h>
#include <cos_synchronization.h>
#define printf printc

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

int delay(void) {
	int i, j;
	volatile int mem = 0;

	for (i = 0; i < 10; i++) {
		for (j = 0; j < 10; j++) {
			mem++;
		}
	}

	return 0;
}

#define ITER 100
#define LOOP NUM_CPU_COS
unsigned long long time[ITER], time1[ITER];
int core_access[NUM_CPU_COS];

int meas(void)
{
	unsigned long long s, e;
	volatile int a, b = 0;
	int i, j, my_id;

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

	for (i = 0; i < ITER; i++) {
		if (b > 100) break;

		rdtscll(s);
#pragma omp parallel for
		for (j = 0; j < LOOP; j++)
		{
			if (omp_get_thread_num() == 0) {
				rdtscll(e);
				time1[i] = e - s;
			}

			//core_access[cos_cpuid()] = 1;
		}
		rdtscll(e);
		time[i] = e - s;
		//timed_event_block(cos_spd_id(), 1);		//usleep(1000000);
		delay();
	}
	/* for (i = 0; i < NUM_CPU; i++) { */
	/* 	printf("core %d: %d\n", i, core_access[i]); */
	/* } */
	unsigned long long sum = 0;
	for (i = 0; i < ITER; i++) {
		printc("cost %d: %llu   half way %llu\n", i, time[i], time1[i]);
		sum += time[i];
	}
	sum /= ITER;
	printc("average %llu\n", sum);

	/* for (i = 0; i < NUM_CPU_COS; i++) { */
	/* 	printc("for core i %d, access %d\n", i, core_access[i]); */
	/* } */

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
