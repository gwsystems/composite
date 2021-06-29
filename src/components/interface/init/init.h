#ifndef INIT_H
#define INIT_H

/***
 * This interface is the lowest-level functionality for
 * coordinating/synchronizing initialization. Components that are
 * dependent on others should be initialized first. Additionally, it
 * provides calls to terminate execution, and orchestrate parallel
 * initialization.
 *
 * This API should *not* used directly, as it is driven by the runtime
 * in the `component` library.
 */

#include <cos_stubs.h>

/**
 * This function signifies that a phase of initialization is complete
 * (done), but that execution should continue (e.g. into a `main`) if
 * the component wishes it. This
 *
 * 1. enables the controlled initialization of components (so that
 *    parents are always initialized before children), and
 * 2. backwards compatibility as `main` can still execute if such
 *    synchronization isn't necessary.
 *
 * `cos_component.c` does all of this transparently: if you provide a
 * `cos_init`, it will run it, if you provide a `main`, this will be
 * called in the appropriate manner.
 *
 * - @parallel_init - Do we wish for the parallel initialization
 *                    sequence to commence?
 * - @cont - When initialization is done, which main should execute?
 */
typedef enum
{
	INIT_MAIN_NONE = 0, /* no continued execution */
	INIT_MAIN_SINGLE,   /* only initialization core executes main */
	INIT_MAIN_PARALLEL  /* all cores execute a parallel main */
} init_main_t;

void init_done(int parallel_init, init_main_t cont);
/***/
void COS_STUB_DECL(init_done)(int parallel_init, init_main_t cont);

/**
 * This function denotes that this component is done executing. This
 * is called instead of UNIX's `exit` on return from `main`. It can be
 * called more explicitly as well. A component should *not* exit,
 * unless all components with transitive dependencies on it have
 * exited. In practice, this means that most system-level components
 * should not exit. Note that if you don't provide a `main`, this is
 * never automatically called.
 *
 * This will also be called by *each* parallel thread if parallel_main
 * is defined. The component should be interpreted as having excited
 * iff the last thread calls this function. In this case, the retval
 * is meaningless.
 *
 * - @retval - the return value we're exiting with
 */
void init_exit(int retval) __attribute__((noreturn));
/***/
void COS_STUB_DECL(init_exit)(int retval) __attribute__((noreturn));

/**
 * This function enables you to retrieve the number of cores this
 * component can and will execute on. This function is always library
 * defined (see `lib.c`), so will never result in a synchronous
 * invocation.
 */
int init_parallelism(void);

/**
 * This function ensures that execution only proceeds past this if
 * cos_init has been executed on the initial core. Effectively a
 * barrier for parallel execution. Also implemented as a library to
 * reduce sinv capability consumption.
 */
void init_parallel_await_init(void);
/***/
void COS_STUB_DECL(init_parallel_await_init)(void);

#endif /* INIT_H */
