#include <cos_component.h>
#include <cos_stubs.h>
#include <memmgr.h>

COS_CLIENT_STUB(cbuf_t, memmgr_shared_page_allocn, unsigned long num_pages, vaddr_t *pgaddr)
{
	COS_CLIENT_INVCAP;
	word_t unused, addrret;
	cbuf_t ret;

	ret = cos_sinv_2rets(uc->cap_no, num_pages, 0, 0, 0, &addrret, &unused);
	*pgaddr = addrret;

	return ret;
}

COS_CLIENT_STUB(unsigned long, memmgr_shared_page_map, cbuf_t id, vaddr_t *pgaddr)
{
	COS_CLIENT_INVCAP;
	word_t unused, addrret;
	unsigned long ret;

	ret = cos_sinv_2rets(uc->cap_no, id, 0, 0, 0, &addrret, &unused);
	*pgaddr = addrret;

	return ret;
}
