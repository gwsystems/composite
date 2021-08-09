#include <cos_component.h>
#include <cos_stubs.h>
#include <filesystem.h>
#include <memmgr.h>

vaddr_t *__filesystem_c_smem_pool;

CCTOR int
__filesystem_c_smem_init()
{
	int     ret;
	cbuf_t  cid;
	vaddr_t p;

	cid = memmgr_shared_page_allocn(2, &p);

	ret = memmgr_shared_page_map(cid, &__filesystem_c_smem_pool);
	if (!ret) { return ret; }

	ret = __filesystem_s_smem_init(cid);
	if (ret) { return ret; }

	return 0;
}

COS_CLIENT_STUB(word_t, filesystem_fopen)(struct usr_inv_cap *uc, const char *path, const char *flags)
{
	word_t unused;

	memcpy(__filesystem_c_smem_pool, path, 4096);
	memcpy((char *)__filesystem_c_smem_pool + 4096, flags, 4096);

	return cos_sinv_2rets(uc->cap_no, 0, 0, 0, 0, &unused, &unused);
}

COS_CLIENT_STUB(size_t, filesystem_fread)(struct usr_inv_cap *uc, word_t fd, void *buf, size_t size)
{
	word_t        unused;
	int           c, s;
	unsigned long ret;

	for (c = 0, s = size; s > 0; s -= 4096, c++) {
		// to be fix
		ret = cos_sinv_2rets(uc->cap_no, fd, 0, 4096, 0, &unused, &unused);
		if (!ret) {
			//BUG();
			return ret;
		}

		// printc("read stub buf: %s\n", __blockdev_c_smem_pool);
		// printc("read offset: %lu\n", offset + c);
		memcpy((char *)buf + c * 4096, __filesystem_c_smem_pool, 4096);
	}

	return ret;
}

COS_CLIENT_STUB(int, filesystem_fwrite)(struct usr_inv_cap *uc, word_t fd, void *buf, size_t size)
{
	word_t        unused;
	int           c, s;
	unsigned long ret;


	for (c = 0, s = size; s > 0; s -= 4096, c++) {
		// to be fix
		memcpy(__filesystem_c_smem_pool, (char *)buf + c * 4096, 4096);

		ret = cos_sinv_2rets(uc->cap_no, fd, 0, 4096, 0, &unused, &unused);
		if (!ret) {
			//BUG();
			return ret;
		}
	}

	return ret;
}