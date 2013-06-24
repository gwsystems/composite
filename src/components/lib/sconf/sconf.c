#include <stdlib.h>
#include <string.h>

#include "sconf.h"

static char *sconf_errvals[6] = {
	"success", "not enough tokens", "invalid string", 
	"incomplete string", "index out of bounds", "key not found"
};

char *
sconf_err_str(sconf_ret_t r)
{ return (r > 0 || r < -5) ? "illegal return value" : sconf_errvals[r * -1]; }

void 
sconf_init(struct sconf *sc, const char *js, unsigned int slen, jsmntok_t *tokens, unsigned int ntokens) 
{
	jsmn_init(&sc->jp);
	sc->tokens    = tokens;
	sc->ntokens   = ntokens;
	sc->js        = js;
	sc->slen      = slen;
	sc->type      = 0;
	sc->tok_start = 0;
	sc->tok_end   = 0;
}

/**
 * Return a positive integer for the # of results, or a negative
 * number which should be interpreted as a JSMN_ERROR_*.
 */
int
sconf_parse(struct sconf *sc)
{
	jsmnerr_t r;
	jsmntok_t *ts;
	jsmntype_t t;
	unsigned int ntok;
	const char *js = sc->js;
	unsigned int len = sc->slen;

	ts = sc->tokens;
	if ((r = jsmn_parsen(&sc->jp, js, len, ts, sc->ntokens, &ntok)) < 0) return r;
	t = ts[0].type;
	if (t == JSMN_ARRAY) sc->type = SCONF_ARRAY;
	else                 sc->type = SCONF_OBJ;

	sc->tok_start = 0;
	sc->tok_end   = ntok-1;
	if (t == JSMN_ARRAY || t == JSMN_OBJECT) {
		sc->tok_start++;
		ntok--;
	}

	return ntok;
}

static inline sconf_ret_t
__sconf_index(struct sconf *sc, jsmntype_t expected, int idx, jsmntok_t **tok)
{
	jsmntok_t *t;
	unsigned int off;

	if (sc->type != expected) return SCONF_ERR_INVAL;
	off = sc->tok_start + idx;
	if (off > sc->tok_end) return SCONF_ERR_OOB;
	t = &sc->tokens[off];
	if (t->type == JSMN_OBJECT || 
	    t->type == JSMN_ARRAY) return SCONF_ERR_INVAL;

	*tok = t;
	return SCONF_SUCCESS;
}

/** 
 * Query functions for arrays and simple key/value structures.
 * Note: None of these return a null-terminated string.
 */

sconf_ret_t
sconf_arr_str(struct sconf *sc, int idx, const char *str[], int *slen)
{
	jsmntok_t *t;
	int ret;

	if ((ret = __sconf_index(sc, SCONF_ARRAY, idx, &t)) != 0) return ret;
	*str  = &sc->js[t->start];
	*slen = t->end - t->start;
	return SCONF_SUCCESS;
}

sconf_ret_t
sconf_arr_int(struct sconf *sc, int idx, int *ret)
{
	jsmntok_t *t;
	sconf_ret_t r;

	if ((r = __sconf_index(sc, SCONF_ARRAY, idx, &t)) != 0) return r;
	*ret = atoi(&sc->js[t->start]);
	return SCONF_SUCCESS;
}

sconf_ret_t
sconf_kv_idx(struct sconf *sc, int idx, 
	     const char *rkey[], int *klen, const char *rval[], int *vlen)
{
	jsmntok_t *t;
	int ret;

	idx *= 2;
	if ((ret = __sconf_index(sc, SCONF_OBJ, idx, &t)) != 0) return ret;
	*rkey = &sc->js[t->start];
	*klen = t->end - t->start;

	if ((ret = __sconf_index(sc, SCONF_OBJ, idx+1, &t)) != 0) return ret;
	*rval = &sc->js[t->start];
	*vlen = t->end - t->start;

	return SCONF_SUCCESS;
}

/**
 * Iterates through all keys, so not the fastest thing.  Might want to
 * consider not doing this in performance-centric code.  Easy to use,
 * not fast!
 */
sconf_ret_t
sconf_kv_lookup(struct sconf *sc, const char *key, int klen, const char *rval[], int *vlen)
{
	jsmntok_t *t;
	unsigned int i;

	if (sc->type != SCONF_OBJ) return JSMN_ERROR_INVAL;
	for (i = sc->tok_start ; i+1 <= sc->tok_end ; i = i + 2) {
		int slen;

		t = &sc->tokens[i];
		if (t->type == JSMN_OBJECT || 
		    t->type == JSMN_ARRAY) return SCONF_ERR_INVAL;

		slen = (klen > (t->end - t->start)) ? klen : t->end - t->start;
		/* found it! */
		if (!strncmp(key, &sc->js[t->start], slen) && key[slen] == '\0') {
			*rval = &sc->js[sc->tokens[i+1].start];
			*vlen = sc->tokens[i+1].end - sc->tokens[i+1].start;
			return SCONF_SUCCESS;
		}
	}
	
	*rval = NULL;
	return SCONF_ERR_NOTFND;
}

int 
sconf_kv_populate(char *str, struct sconf_kvs *kvs, int nkvs)
{
	struct sconf sc;
	sconftok_t ts[32];
	sconf_ret_t r = SCONF_SUCCESS;
	int i;

	sconf_init(&sc, str, strlen(str)+1, ts, 32);
	if ((r = sconf_parse(&sc)) < 0) return r;
	
	for (i = 0 ; i < nkvs ; i++) {
		if (sconf_kv_lookup(&sc, kvs[i].key, strlen(kvs[i].key), 
				    &kvs[i].val, &kvs[i].vlen)) continue;
	}

	return 0;
}
