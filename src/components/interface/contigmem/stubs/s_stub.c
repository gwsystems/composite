#include <cos_stubs.h>
#include <contigmem.h>

COS_SERVER_3RET_STUB(cbuf_t, contigmem_shared_alloc_aligned)
{
	return contigmem_shared_alloc_aligned(p0, p1, r1);
}
