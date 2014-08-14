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
	cbufp_t cb;
	char *addr;
	addr = cbufp_alloc(sz, &cb);
	assert(addr);
	assert(cb);
	addr[0] = '_';
	cbufp_send(cb);
	return cb;
}

void unit_cbufp_deref(cbufp_t cb)
{
	cbufp_deref(cb);
}

void cos_init(void)
{
	return;
}
