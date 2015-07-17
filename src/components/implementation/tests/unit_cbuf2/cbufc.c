#include <cos_component.h>
#include <print.h>
#include <unit_cbuf.h>
#include <cbuf.h>

void cos_fix_spdid_metadata(spdid_t the_spd)
{
	printc("fixing metadata in unit_cbuf2\n");
}

void unit_cbuf(cbuf_t cbuf, int sz)
{
	char *c = cbuf2buf(cbuf, sz);
	cbuf_t cb;
	char *addr;

	assert(c);
	assert(c[0] == '_');

	addr = cbuf_alloc_ext(sz, &cb, CBUF_TMEM);
	cbuf_free(cb);
}
