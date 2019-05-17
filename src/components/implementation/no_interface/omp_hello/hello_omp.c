#include <llprint.h>
#include <omp.h>

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
#pragma omp parallel private(id)
  {
#pragma omp for
  for (id = 0; id < 10; id++) 
  {
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
