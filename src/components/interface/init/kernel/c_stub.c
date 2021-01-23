#include <cos_component.h>
#include <ps.h>
#include <init.h>
#include <barrier.h>
#include <cos_stubs.h>

/* want to keep this state small */
static unsigned long         init_await_parallel_activation = 1;
static coreid_t              init_core                      = 0;
static struct simple_barrier init_barrier                   = SIMPLE_BARRIER_INITVAL;

/*
 * These functions are library-defined (no sinvs to other components),
 * BUT we may want to define them in the component that also depends
 * on this interface. How do we differentiate between calling the
 * function init_done(...) when we mean to call this function, from
 * the init_done defined within the component? This macro makes this
 * definition of init_done weak, and aliases it to another _extern_
 * symbol. In the component, we can use COS_EXTERN_INV...to call
 * this. See cos_component.c for the application of this idea.
 */
void COS_STUB_LIBFN(init_done)(int parallel_init, init_main_t main_type)
{
	static unsigned long first = 1;

	if (ps_cas(&first, 1, 0)) { ps_store(&init_core, cos_coreid()); }

	/* only the initial thread will call with parallel_init == 1 */
	if (parallel_init) {
		simple_barrier_init(&init_barrier, init_parallelism());
		ps_mem_fence();
		ps_store(&init_await_parallel_activation, 0);

		return;
	}
	ps_store(&init_await_parallel_activation, 0);

	if (main_type == INIT_MAIN_NONE) {
		/* TODO give back stack */
		COS_EXTERN_INV(init_exit)(0);
		/* should not get here */

		return;
	}

	simple_barrier(&init_barrier);
	if (main_type == INIT_MAIN_SINGLE && ps_load(&init_core) != cos_coreid()) {
		/* TODO give back stack */
		COS_EXTERN_INV(init_exit)(0);

		return;
		/* only the initial core proceeds in this case... */
	}
	/* continue on to main execution */

	return;
}
COS_STUB_ALIAS(init_done);

void COS_STUB_LIBFN(init_exit)(int retval)
{
	/* Don't really need to differentiate between parallel or not here... */
	while (1)
		;
}
COS_STUB_ALIAS(init_exit);

void COS_STUB_LIBFN(init_parallel_await_init)(void)
{
	while (ps_load(&init_await_parallel_activation))
		;
}
COS_STUB_ALIAS(init_parallel_await_init);
