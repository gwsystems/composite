#include <cos_kernel_api.h>
#include <cos_types.h>


int
parallel_main(coreid_t cid)
{
	assert(cid == cos_coreid());

	/* prints need to be serialized for this test to be useful */
	printc("core %d SUCCESS\n", cos_coreid());

	return 0;
}
