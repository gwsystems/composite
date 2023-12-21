#include <cos_stubs.h>
#include <memmgr.h>

COS_SERVER_3RET_STUB(cbuf_t, memmgr_shared_page_allocn)
{
	return memmgr_shared_page_allocn(p0, r1);
}

COS_SERVER_3RET_STUB(cbuf_t, memmgr_shared_page_allocn_aligned)
{
	return memmgr_shared_page_allocn_aligned(p0, p1, r1);
}

COS_SERVER_3RET_STUB(unsigned long, memmgr_shared_page_map)
{
	return memmgr_shared_page_map(p0, r1);
}

COS_SERVER_3RET_STUB(unsigned long, memmgr_shared_page_map_aligned)
{
	return memmgr_shared_page_map_aligned(p0, p1, r1);
}

COS_SERVER_3RET_STUB(unsigned long, memmgr_shared_page_map_aligned_in_vm)
{
	return memmgr_shared_page_map_aligned_in_vm(p0, p1, r1, p2);
}
