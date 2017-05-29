#include <cos_component.h>
#include <print.h>
#include <cbuf.h>

cbuf_t 
f(cbuf_t cb, int len)
{
	char *b;

	printc("\n****** BOT: thread %d in spd %ld ******\n",cos_get_thd_id(), cos_spd_id());
	b = cbuf2buf(cb, len);

	if (!b) {
		printc("WTF\n");
		return cbuf_null();
	}
	/* check_val(); */

	memset(b, 'b', len);

	return cb;
}

void check_val(){
	int i;
	printc("spd %ld\n",cos_spd_id());
	for(i=0;i<10;i++)
		printc("check:::i:%d %p\n",i,cvect_lookup(&meta_cbuf, i));


}
