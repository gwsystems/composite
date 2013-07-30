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

        assert(param && len >= 0);
        assert(param[len] == '\0');

	d = cbuf_alloc(sz, &cb);
	if (!d) return -6;

        d->tid    = tid;
	d->tflags = tflags;
	d->evtid  = evtid;
        d->len[0] = 0;
        d->len[1] = len;
	memcpy(&d->data[0], param, len + 1);

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

CSTUB_FN_ARGS_4(int, treadp, spdid_t, spdid, td_t, td, int *, off, int *, len)
//	CSTUB_ASM_RET_PRE(*off, *len)
	__asm__ __volatile__( \
		"pushl %%ebp\n\t" \
		"movl %%esp, %%ebp\n\t" \
		"movl $1f, %%ecx\n\t" \
		"sysenter\n\t" \
		".align 8\n\t" \
		"jmp 2f\n\t" \
		".align 8\n\t" \
		"1:\n\t" \
		"popl %%ebp\n\t" \
		"movl $0, %%ecx\n\t" \
	        "movl %%esi, %%ebx\n\t" \
	        "movl %%edi, %%edx\n\t" \
		"jmp 3f\n\t" \
		"2:\n\t" \
		"popl %%ebp\n\t" \
		"movl $1, %%ecx\n\t" \
	        "movl %%esi, %%ebx\n\t" \
	        "movl %%edi, %%edx\n\t" \
		"3:" \
	        : "=a" (ret), "=c" (fault), "=b" (*off), "=d" (*len)
		: "a" (uc->cap_no), "b" (spdid), "S" (td)
		: "edi", "memory", "cc");
CSTUB_POST

CSTUB_4(int, tread, spdid_t, td_t, int, int);
CSTUB_4(int, twrite, spdid_t, td_t, int, int);

struct __sg_trmeta_data {
        td_t td;
        int klen, retval_len;
        char data[0];
};
CSTUB_FN_ARGS_6(int, trmeta, spdid_t, spdid, td_t, td, const char *, key, unsigned int, klen, char *, retval, unsigned int, retval_len)
        cbuf_t cb;
        int sz = sizeof(struct __sg_trmeta_data) + klen + retval_len + 1;
        struct __sg_trmeta_data *d;

        assert(key && retval && klen > 0 && retval_len > 0);
        assert(key[klen] == '\0' && sz <= PAGE_SIZE);

        d = cbuf_alloc(sz, &cb);
        if (!d) return -1;

        d->td = td;
        d->klen = klen;
        d->retval_len = retval_len;
        memcpy(&d->data[0], key, klen + 1);

CSTUB_ASM_3(trmeta, spdid, cb, sz)

        if (ret >= 0) {
                if ((unsigned int)ret > retval_len) { // as ret >= 0, cast it to unsigned int to omit compiler warning
                        cbuf_free(d);
                        return -EIO;
                }
                memcpy(retval, &d->data[klen + 1], ret + 1);
        }
        cbuf_free(d);
CSTUB_POST


struct __sg_twmeta_data {
        td_t td;
        int klen, vlen;
        char data[0];
};
CSTUB_FN_ARGS_6(int, twmeta, spdid_t, spdid, td_t, td, const char *, key, unsigned int, klen, const char *, val, unsigned int, vlen)
        cbuf_t cb;
        int sz = sizeof(struct __sg_twmeta_data) + klen + vlen + 1;
        struct __sg_twmeta_data *d;

        assert(key && val && klen > 0 && vlen > 0);
        assert(key[klen] == '\0' && val[vlen] == '\0' && sz <= PAGE_SIZE);

        d = cbuf_alloc(sz, &cb);
        if (!d) assert(0); //return -1;

        d->td = td;
        d->klen = klen;
        d->vlen = vlen;
        memcpy(&d->data[0], key, klen + 1);
        memcpy(&d->data[klen + 1], val, vlen + 1);

CSTUB_ASM_3(twmeta, spdid, cb, sz)

        cbuf_free(d);
CSTUB_POST
