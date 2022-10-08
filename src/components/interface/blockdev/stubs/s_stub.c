#include <cos_stubs.h>
#include <blockdev.h>

vaddr_t *__blockdev_s_smem_pool[64];

COS_SERVER_3RET_STUB(unsigned long, blockdev_bread)
{
	compid_t token;
	token = (compid_t)cos_inv_token();

	return blockdev_bread(__blockdev_s_smem_pool[token], p1, p2);
}

COS_SERVER_3RET_STUB(unsigned long, blockdev_bwrite)
{
	compid_t token;
	token = (compid_t)cos_inv_token();

	return blockdev_bwrite(__blockdev_s_smem_pool[token], p1, p2);
}

int
__blockdev_s_smem_init(cbuf_t cid)
{
	compid_t token;
	token = (compid_t)cos_inv_token();

	return memmgr_shared_page_map(cid, (vaddr_t *)&__blockdev_s_smem_pool[token]);
}
