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
