#include <cos_component.h>
#include <print.h>
#include <cbuf.h>
#include <cbufp.h>
#include <unit_cbufp.h>

void unit_cbufp2buf(cbufp_t cbuf, int sz)
{
	char *c = cbufp2buf(cbuf, sz);
	assert(!c);
}

cbufp_t unit_cbufp_alloc(int sz)
{
	cbufp_t cbuf;
	char *addr;
	addr = cbufp_alloc(sz, &cbuf);
	assert(addr);
	assert(cbuf);
	addr[0] = '_';
	cbufp_send(cbuf);
	return cbuf;
}

void unit_cbufp_deref(cbufp_t cbuf, int sz)
{
	cbufp_deref(cbuf);
}

int unit_cbufp_map_at(cbufp_t cbuf, int sz, spdid_t spdid, vaddr_t buf)
{
	vaddr_t d = cbufp_map_at(cos_spd_id(), cbuf, spdid, buf, MAPPING_RW);
	if ( d != buf ) return -EINVAL;
	return 0;
}

int unit_cbufp_unmap_at(cbufp_t cbuf, int sz, spdid_t spdid, vaddr_t buf)
{
	cbufp_unmap_at(cos_spd_id(), cbuf, spdid, buf);
	return 0;
}

void cos_init(void)
{
	return;
}
