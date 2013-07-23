#include <cos_component.h>
#include <print.h>

#include <mem_mgr_large.h>
#include <cos_alloc.h>
#include <sched.h>

#include <parallel_inv.h>

// printf ? 
extern struct intra_ainv_info *thd_intra_ainv[MAX_NUM_THREADS];

int omp_get_thread_num() {
	/* This means nested fork/join. OMP returns 0
	 * as thread number in this case. I don't
	 * change it to 0 here as I don't want to
	 * overwrite the original number. */

	return ainv_get_thd_num();
}

int omp_get_num_threads() {
	return ainv_get_num_thds();
}

int GOMP_parallel_start (void *fn, void *data, unsigned int num_threads) {
	printc("core %ld, gomp start: fn %d, data %d, go parallel %u\n", cos_cpuid(), (int)fn, (int)data, num_threads);
	if (num_threads == 1) return 0; /* means no parallelism. */
	
	if (num_threads == 0)
		ainv_parallel_start(fn, data, NUM_CPU_COS);
	else
		ainv_parallel_start(fn, data, num_threads);

	return 0;
}

int GOMP_parallel_end() {
	ainv_parallel_end();

	return 0;
}

int main(void);

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	/* case COS_UPCALL_BRAND_EXEC: */
	/* { */
	/* 	cos_upcall_exec(arg1); */
	/* 	break; */
	/* } */
	case COS_UPCALL_BOOTSTRAP:
	{
		static int first = 1;
		/* if (first) {  */
		/* 	first = 0;  */
		/* 	__alloc_libc_initilize();  */
		/* 	constructors_execute(); */
		/* } */
		/* cos_init(arg1); */
		if (first) {
			first = 0;
			printc("cpu %d, thd %d calling omp main!\n", cos_cpuid(), cos_get_thd_id());
			main(); //
			printc("cpu %d, thd %d omp main done!\n", cos_cpuid(), cos_get_thd_id());
		} else {
			cos_intra_ainv_handling();
		}
		break;
	}
	default:
		/* fault! */
		*(int*)NULL = 0;
		return;
	}
	return;
}

