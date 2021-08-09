#include <cos_component.h>
#include <cos_stubs.h>
#include <blockdev.h>
#include <memmgr.h>

vaddr_t *__blockdev_c_smem_pool;

CCTOR int
__blockdev_c_smem_init()
{
	int     ret;
	cbuf_t  cid;
	vaddr_t p;

	cid = memmgr_shared_page_allocn(1, &p);

	ret = memmgr_shared_page_map(cid, &__blockdev_c_smem_pool);
	if (!ret) { return ret; }

	ret = __blockdev_s_smem_init(cid);
	if (ret) { return ret; }

	return 0;
}

COS_CLIENT_STUB(unsigned long, blockdev_bread)
(struct usr_inv_cap *uc, const void *buf, unsigned long offset, unsigned long length)
{
	word_t        unused;
	int           c;
	unsigned long ret;

	for (c = 0; length > 0; length--, c++) {
		ret = cos_sinv_2rets(uc->cap_no, 0, offset + c, 1, 0, &unused, &unused);
		if (ret) {
			BUG();
			return ret;
		}

		// printc("read stub buf: %s\n", __blockdev_c_smem_pool);
		// printc("read offset: %lu\n", offset + c);
		memcpy((char *)buf + c * 4096, __blockdev_c_smem_pool, 4096);
	}

	return 0;
}

COS_CLIENT_STUB(unsigned long, blockdev_bwrite)
(struct usr_inv_cap *uc, const void *buf, unsigned long offset, unsigned long length)
{
	word_t        unused;
	int           c;
	unsigned long ret;

	for (c = 0; length > 0; length--, c++) {
		memcpy(__blockdev_c_smem_pool, (char *)buf + c * 4096, 4096);
		// printc("write stub buf: %s\n", __blockdev_c_smem_pool);
		// printc("write offset: %lu\n", offset + c);

		ret = cos_sinv_2rets(uc->cap_no, 0, offset + c, 1, 0, &unused, &unused);
		if (ret) {
			BUG();
			return ret;
		}
	}

	return 0;
}