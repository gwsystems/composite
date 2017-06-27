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

CSTUB_FN(td_t, tsplit)(struct usr_inv_cap *uc,
		       spdid_t spdid, td_t tid, char * param,
		       int len, tor_flags_t tflags, long evtid)
{
	long fault = 0;
	td_t ret;
	struct __sg_tsplit_data *d;
	cbuf_t cb;
	int sz = len + sizeof(struct __sg_tsplit_data);

        assert(param && len >= 0);
        assert(param[len] == '\0');

	d = cbuf_alloc_ext(sz, &cb, CBUF_TMEM);
	if (!d) return -6;

        d->tid    = tid;
	d->tflags = tflags;
	d->evtid  = evtid;
        d->len[0] = 0;
        d->len[1] = len;
	memcpy(&d->data[0], param, len + 1);
	cbuf_send(cb);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

	cbuf_free(cb);
	return ret;
}


struct __sg_tmerge_data {
	td_t td;
	td_t td_into;
	int len[2];
	char data[0];
};

CSTUB_FN(int, tmerge)(struct usr_inv_cap *uc,
		      spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	int ret;
	long fault = 0;
	struct __sg_tmerge_data *d;
	cbuf_t cb;
	int sz = len + sizeof(struct __sg_tmerge_data);

        assert(param && len > 0);
	assert(param[len-1] == '\0');

	d = cbuf_alloc_ext(sz, &cb, CBUF_TMEM);
	if (!d) return -1;

	d->td = td;
	d->td_into = td_into;
        d->len[0] = 0;
        d->len[1] = len;
	memcpy(&d->data[0], param, len);
	cbuf_send(cb);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

	cbuf_free(cb);
	return ret;
}

CSTUB_FN(int, treadp)(struct usr_inv_cap *uc,
		      spdid_t spdid, td_t td, int *off, int *sz)
{
	int ret;
	long fault = 0;
	CSTUB_INVOKE_3RETS(ret, fault, *off, *sz, uc, 2, spdid, td);
	return ret;
}

CSTUB_FN(int, tread)(struct usr_inv_cap *uc,
		     spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret;
	long fault = 0;
	CSTUB_INVOKE(ret, fault, uc, 4, spdid, td, cbid, sz);
	return ret;
}

CSTUB_FN(int, twrite)(struct usr_inv_cap *uc,
		      spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret;
	long fault = 0;
	CSTUB_INVOKE(ret, fault, uc, 4, spdid, td, cbid, sz);
	return ret;
}

struct __sg_trmeta_data {
	td_t td;
	int klen, retval_len;
	char data[0];
};

CSTUB_FN(int, trmeta)(struct usr_inv_cap *uc,
		      spdid_t spdid, td_t td, const char *key,
		      unsigned int klen, char *retval, unsigned int max_rval_len)
{
	int ret;
	long fault = 0;
	cbuf_t cb;
	int sz = sizeof(struct __sg_trmeta_data) + klen + max_rval_len + 1;
	struct __sg_trmeta_data *d;

	assert(key && retval && klen > 0 && max_rval_len > 0);
	assert(key[klen] == '\0' && sz <= PAGE_SIZE);

	d = cbuf_alloc_ext(sz, &cb, CBUF_TMEM);
	if (!d) return -1;

	d->td = td;
	d->klen = klen;
	d->retval_len = max_rval_len;
	memcpy(&d->data[0], key, klen + 1);
	cbuf_send(cb);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

	if (ret >= 0) {
		if ((unsigned int)ret > max_rval_len) { // as ret >= 0, cast it to unsigned int to omit compiler warning
			cbuf_free(cb);
			return -EIO;
		}
		memcpy(retval, &d->data[klen + 1], ret + 1);
	}
	cbuf_free(cb);

	return ret;
}

struct __sg_twmeta_data {
	td_t td;
	int klen, vlen;
	char data[0];
};

CSTUB_FN(int, twmeta)(struct usr_inv_cap *uc,
		      spdid_t spdid, td_t td, const char *key,
		      unsigned int klen, const char *val, unsigned int vlen)
{
	int ret;
	long fault = 0;
	cbuf_t cb;
	int sz = sizeof(struct __sg_twmeta_data) + klen + vlen + 1;
	struct __sg_twmeta_data *d;

	assert(key && val && klen > 0 && vlen > 0);
	assert(key[klen] == '\0' && val[vlen] == '\0' && sz <= PAGE_SIZE);

	d = cbuf_alloc_ext(sz, &cb, CBUF_TMEM);
	if (!d) assert(0); //return -1;

	d->td = td;
	d->klen = klen;
	d->vlen = vlen;
	memcpy(&d->data[0], key, klen + 1);
	memcpy(&d->data[klen + 1], val, vlen + 1);
	cbuf_send(cb);

	CSTUB_INVOKE(ret, fault, uc, 3, spdid, cb, sz);

	cbuf_free(cb);

	return ret;
}
