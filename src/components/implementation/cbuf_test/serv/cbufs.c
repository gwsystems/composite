#include <cos_component.h>
#include <print.h>
#include <cbuf.h>

cbuf_t 
f(cbuf_t cb, int len)
{
	char *b;

	b = cbuf2buf(cb, len);
	if (!b) {
		printc("WTF\n");
		return cbuf_null();
	}

//	memset(b, 'b', len);
	
	return cb;
}
