#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <sl.h>
#include <cos_omp.h>

/******************************************************************************/

int main ( void )

/******************************************************************************/
/*
  Purpose:

    HELLO has each thread print out its ID.

  Discussion:

    HELLO is a "Hello, World" program for OpenMP.

  Licensing:

    This code is distributed under the GNU LGPL license. 

  Modified:

    23 June 2010

  Author:

    John Burkardt
*/
{
  int id;
  double wtime;

  PRINTC ( "\n" );
  PRINTC ( "HELLO_OPENMP\n" );
  PRINTC ( "  C/OpenMP version\n" );

  PRINTC ( "\n" );
  PRINTC ( "  Number of processors available = %d\n", omp_get_num_procs ( ) );
  PRINTC ( "  Number of threads =              %d\n", omp_get_max_threads ( ) );

  wtime = omp_get_wtime ( );

  PRINTC ( "\n" );
  PRINTC ( "  OUTSIDE the parallel region.\n" );
  PRINTC ( "\n" );

  id = omp_get_thread_num ( );
  PRINTC ( "  HELLO from process %d\n", id ) ;

  PRINTC ( "\n" );
  PRINTC ( "  Going INSIDE the parallel region:\n" );
  PRINTC ( "\n" );
/*
  INSIDE THE PARALLEL REGION, have each thread say hello.
*/
#if 1
#pragma omp parallel
  {
#pragma omp for schedule(dynamic)
  for (id = 0; id < 10; id++) {
	  PRINTC("id:%u\n", id);
  }
  }
#else
# pragma omp parallel\
  private ( id )
  {
    id = omp_get_thread_num ( );
    PRINTC ("  Hello from process %d\n", id );
  }
#endif
/*
  Finish up by measuring the elapsed time.
*/
  wtime = omp_get_wtime ( ) - wtime;

  PRINTC ( "\n" );
  PRINTC ( "  Back OUTSIDE the parallel region.\n" );
/*
  Terminate.
*/
  PRINTC ( "\n" );
  PRINTC ( "HELLO_OPENMP\n" );
  PRINTC ( "  Normal end of execution.\n" );

  PRINTC ( "\n" );
  PRINTC ( "  Elapsed wall clock time = %f\n", wtime );

  return 0;
}

static void 
cos_main(void *d)
{
	main();

	while (1);
}

extern void cos_gomp_init(void);

void
cos_init(void *d)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
	int i;
	static volatile unsigned long first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };

	if (ps_cas((unsigned long *)&first, NUM_CPU + 1, cos_cpuid())) {
		PRINTC("In OpenMP-based Hello Program!\n");
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();
	} else {
		while (!ps_load((unsigned long *)&init_done[first])) ;

		cos_defcompinfo_sched_init();
	}
	sl_init(SL_MIN_PERIOD_US*100);
	ps_faa((unsigned long *)&init_done[cos_cpuid()], 1);

	/* make sure the INITTHD of the scheduler is created on all cores.. for cross-core sl initialization to work! */
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load((unsigned long *)&init_done[i])) ;
	}

	if (!cos_cpuid()) {
		struct sl_thd *t = NULL;

		cos_gomp_init();

		t = sl_thd_alloc(cos_main, NULL);
		assert(t);
		sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, TCAP_PRIO_MAX));
	}

	sl_sched_loop_nonblock();

	PRINTC("Should never get here!\n");
	assert(0);
}
