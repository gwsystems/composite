#include <cos_stubs.h>
#include <filesystem.h>

vaddr_t *__filesystem_s_smem_pool[64];

COS_SERVER_3RET_STUB(word_t, filesystem_fopen)
{
	compid_t token;
	token = (compid_t)cos_inv_token();

	return filesystem_fopen((const char *)__filesystem_s_smem_pool[token],
	                        (const char *)__filesystem_s_smem_pool[token] + 4096);
}

COS_SERVER_3RET_STUB(int, filesystem_fread)
{
	compid_t token;
	size_t   ret;

	token = (compid_t)cos_inv_token();

	ret = filesystem_fread(p0, __filesystem_s_smem_pool[token], p2);

	return ret;
}

COS_SERVER_3RET_STUB(int, filesystem_fwrite)
{
	compid_t token;
	size_t   ret;

	token = (compid_t)cos_inv_token();

	ret = filesystem_fwrite(p0, __filesystem_s_smem_pool[token], p2);

	return ret;
}

int
__filesystem_s_smem_init(cbuf_t cid)
{
	compid_t token;
	token = (compid_t)cos_inv_token();

	return memmgr_shared_page_map(cid, (vaddr_t *)&__filesystem_s_smem_pool[token]);
}