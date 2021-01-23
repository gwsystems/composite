#include <cos_component.h>
#include <cos_stubs.h>
#include <memmgr.h>

COS_CLIENT_STUB(cbuf_t, memmgr_shared_page_allocn)(struct usr_inv_cap *uc, unsigned long num_pages, vaddr_t *pgaddr)
{
	word_t unused, addrret;
	cbuf_t ret;

	ret     = cos_sinv_2rets(uc->cap_no, num_pages, 0, 0, 0, &addrret, &unused);
	*pgaddr = addrret;

	return ret;
}

COS_CLIENT_STUB(unsigned long, memmgr_shared_page_map)(struct usr_inv_cap *uc, cbuf_t id, vaddr_t *pgaddr)
{
	word_t        unused, addrret;
	unsigned long ret;

	ret     = cos_sinv_2rets(uc->cap_no, id, 0, 0, 0, &addrret, &unused);
	*pgaddr = addrret;

	return ret;
}
