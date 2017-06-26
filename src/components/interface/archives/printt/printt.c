#define PRINT_FN print_null
int print_null(char *s) { return 0; }

#include <cos_component.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <cbuf.h>
#include <torrent.h>

extern td_t print_tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);
extern int print_twrite(spdid_t spdid, td_t td, int cbid, int sz);

static td_t printt_init(void)
{
	td_t init;
	char *pchan = "0";

	init = print_tsplit(cos_spd_id(), td_root, pchan, strlen(pchan), TOR_WRITE, 0);
	assert(init > 0);
	return init;
}

#include <cos_debug.h>
int __attribute__((format(printf,1,2))) printc(char *fmt, ...)
{
	static td_t tor = 0;
	char *s;
	va_list arg_ptr;
	int ret;
	cbuf_t cb = cbuf_null();

	if (!tor) tor = printt_init();

	s = cbuf_alloc_ext(4096, &cb, CBUF_TMEM);
	assert(s);
	va_start(arg_ptr, fmt);
	ret = vsnprintf(s, 4096, fmt, arg_ptr);
	va_end(arg_ptr);
	print_twrite(cos_spd_id(), tor, cb, ret);
	cbuf_free(cb);

	return ret;
}

int prints(char *str)
{
	return printc("%s", str);
}
