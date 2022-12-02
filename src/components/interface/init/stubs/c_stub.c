#include <cos_component.h>
#include <cos_stubs.h>
#include <ps.h>

#include <init.h>

static int awaiting_init = 1;

/*
 * This defines a wrapper around the synchronous invocation for
 * init_done. In this case, this enables us to add the logic for
 * forcing parallel threads to await initialization completion. See
 * the pong interface for a similar usage pattern, but using this code
 * to serialize and deserialize arguments. In contrast, this code
 * doesn't have a corresponding client-side stub as it only
 * orchestrates the coordination with the await_init function.
 */
COS_CLIENT_STUB(int, init_done, int parallel_init, init_main_t cont)
{
	COS_CLIENT_INVCAP;
	int ret;

	ret = cos_sinv(uc, parallel_init, cont, 0, 0);
	ps_store(&awaiting_init, 0);

	return ret;
}

/*
 * This function is implemented as a library compiled directly into
 * the client component.
 */
void
COS_STUB_LIBFN(init_parallel_await_init)(void)
{
	while (ps_load(&awaiting_init)) ;
}
COS_STUB_ALIAS(init_parallel_await_init);
