#include <cos_component.h>
#include <print.h>

#include <mem_mgr_large.h>
#include <cos_alloc.h>
#include <sched.h>

#include <parallel_inv.h>

// printf ? 

int omp_get_thread_num() {
	/* The value is not valid when nested parallel presents. */
	return ainv_get_thd_num();
}

int omp_get_num_threads() {
	/* The value is not valid when nested parallel presents. */
	return ainv_get_num_thds();
}

int omp_get_max_threads() {
	return ainv_get_max_thds();
}

int omp_set_nested(int enable) {
	/* always enable for now. */
	return 0;
}

/* num_threads = 0 means decided by run-time library. 1 means no
 * parallelism. > 1 means max number of threads. */
int 
GOMP_parallel_start (void *fn, void *data, unsigned int num_threads) 
{
	int max_par;
	/* printc("core %ld, gomp start: fn %d, data %d, go parallel %u\n",  */
	/*        cos_cpuid(), (int)fn, (int)data, num_threads); */
	if (unlikely(num_threads == 1)) return 0; /* means no parallelism. */

	if (likely(num_threads == 0)) max_par = NUM_CPU_COS;
	else                          max_par = num_threads;

	ainv_parallel_start(fn, data, max_par);

	return 0;
}

/* chunk_size is used for omp scheduling. Ignored here. */
/* void  */
/* GOMP_parallel_loop_static_start (void (*fn) (void *), void *data, */
/* 				 unsigned num_threads, long start, long end, */
/* 				 long incr, long chunk_size) */
/* { */
/* 	int max_par; */
/* 	printc("core %ld, parallel for start: fn %d, data %d, go parallel %u\n",  */
/* 	       cos_cpuid(), (int)fn, (int)data, num_threads); */
/* 	if (num_threads == 1) return; /\* means no parallelism. *\/ */

/* 	if (num_threads == 0) */
/* 		max_par = NUM_CPU_COS; */
/* 	else */
/* 		max_par = num_threads; */
/* 	ainv_parallel_loop_start(fn, data, max_par, start, end, incr); */

/* 	return; */
/* } */

/* int GOMP_loop_static_next (long *istart, long *iend) */
/* { */
/* //	if () */
/* 	return 0; */
/* } */

void
GOMP_loop_end_nowait (void)
{
	/* nothing need to be done for us. */
	return;
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
		static int first = 1, second = 1;
		/* if (first) {  */
		/* 	first = 0;  */
		/* 	__alloc_libc_initilize();  */
		/* 	constructors_execute(); */
		/* } */
		/* cos_init(arg1); */
		if (first) {
			first = 0;

			union sched_param sp;
			int ret;

			sp.c.type = SCHEDP_PRIO;
			sp.c.value = 10;

			ret = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
			if (!ret) BUG();

			return;
		} else if (second){
			second = 0;

			printc("cpu %ld, thd %d calling omp main in comp %ld!\n", 
			       cos_cpuid(), cos_get_thd_id(), cos_spd_id());
			main(); //
			printc("cpu %ld, thd %d omp main done in comp %ld!\n", 
			       cos_cpuid(), cos_get_thd_id(), cos_spd_id());
			
			return;
		} else {
			cos_intra_ainv_handling();
			return;
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

