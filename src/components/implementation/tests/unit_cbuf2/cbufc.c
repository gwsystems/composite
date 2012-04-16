#include <cos_component.h>
#include <print.h>
#include <unit_cbuf.h>
#include <cbuf.h>

void unit_cbuf(cbuf_t cbuf, int sz)
{
	char *c = cbuf2buf(cbuf, sz);
	cbuf_t cb;
	char *addr;

	assert(c);
	assert(c[0] == '_');
	c[0] = '*';

	addr = cbuf_alloc(sz, &cb);
	cbuf_free(addr);
}
