#include <torrent.h>
#include <cstub.h>
#include <print.h>

struct __sg_tsplit_data {
	td_t tid;
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};
CSTUB_FN_ARGS_6(td_t, tsplit, spdid_t, spdid, td_t, tid, char *, param, int, len, tor_flags_t, tflags, long, evtid)
	struct __sg_tsplit_data *d;
	cbuf_t cb;
	int sz = len + sizeof(struct __sg_tsplit_data);

        assert(param && len > 0);
        assert(param[len-1] == '\0');

	d = cbuf_alloc(sz, &cb);
	if (!d) return -6;

        d->tid = tid;
	d->tflags = tflags;
	d->evtid = evtid;
        d->len[0] = 0;
        d->len[1] = len;
	memcpy(&d->data[0], param, len);

CSTUB_ASM_3(tsplit, spdid, cb, sz)

	cbuf_free(d);
CSTUB_POST


struct __sg_tmerge_data {
	td_t td;
	td_t td_into;
	int len[2];
	char data[0];
};
CSTUB_FN_ARGS_5(int, tmerge, spdid_t, spdid, td_t, td, td_t, td_into, char *, param, int, len)
	struct __sg_tmerge_data *d;
	cbuf_t cb;
	int sz = len + sizeof(struct __sg_tmerge_data);

        assert(param && len > 0);
	assert(param[len-1] == '\0');

	d = cbuf_alloc(sz, &cb);
	if (!d) return -1;

	d->td = td;
	d->td_into = td_into;
        d->len[0] = 0;
        d->len[1] = len;
	memcpy(&d->data[0], param, len);

CSTUB_ASM_3(tmerge, spdid, cb, sz)

	cbuf_free(d);
CSTUB_POST


CSTUB_4(int, tread, spdid_t, td_t, int, int);
CSTUB_4(int, twrite, spdid_t, td_t, int, int);
