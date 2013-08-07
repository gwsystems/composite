#include <omp.h>
#include <stdio.h>

int main() {
	int nthds;
	int i,j,k;
	volatile int my_id;

/* #pragma omp parallel private(my_id) */
/* 	{ */
/* 		my_id = omp_get_thread_num(); */
/* 		printf("omp thread running: my_id is %d\n", my_id); */

/* 		nthds = omp_get_num_threads(); */

/* 		if (my_id == 0) */
/* 			printf("main thread: total thds %d\n", nthds); */

/* 		for (i = 0; i < 5000; i++) { */
/* 			for (j = 0; j < 5000; j++) { */
/* 				if (my_id != omp_get_thread_num()) { */
/* 					printf("private variable test FAIL! thd %d, var %d\n", omp_get_thread_num(), my_id); */
/* 					break; */
/* 				} */
/* 				mem++; */
/* 			} */
/* 		} */

/* 		printf("thd %d (private var %d) spinning done!\n", omp_get_thread_num(), my_id); */
/* 	} */

	volatile int mem = 1;

#pragma omp parallel for private(j)
	for (i = 0; i < 2; i++) {
		omp_set_nested(1);
		/* int ii = 10 * i; */
		printf("thd %d, %d thds, outer loop i %d\n", 
		       omp_get_thread_num(), omp_get_num_threads(), i);
#pragma omp parallel for
		for (j = 0; j < 4; j++) {
			/* int jj = j * j; */
			printf("thd %d, %d thds, inner loop i %d, j %d\n", 
			       omp_get_thread_num(), omp_get_num_threads(), i, j);
		}
		//while (mem > 0);
		printf("thd %d, %d thds, outer loop end i %d\n", 
		       omp_get_thread_num(), omp_get_num_threads(), i);
	}

	return 0;
}
