#ifndef SCONF_H

/**
 * License: see LICENSE.txt, copyright 2013 Gabe Parmer
 */

/**
 * Simple configuration parser that is a wrapper around jsmn.
 * Configuration strings are only of the form (informally):
 *
 * "key0:val0,key1:val1,..." or
 * "{key0:val0,key1:val1,...}" or
 * "[val0,val1,...]"
 * 
 * val? can be either an integer, bool, or a string, though all key?
 * are evaluated as a string (even if it is a string of numbers).
 * White-space is ignored _except_ that commas in the k/v
 * representation can be replaced with \n.
 * 
 * Main parts of json not addressed here: recursive objects/arrays,
 * null values.
 */

#include <jsmn/jsmn.h>

typedef enum {
	SCONF_ARRAY = 1,
	SCONF_OBJ
} sconf_t;

/* positive values typically denote the # of tokens */
typedef enum {
	SCONF_ERR_NOMEM  = JSMN_ERROR_NOMEM,
	SCONF_ERR_INVAL  = JSMN_ERROR_INVAL,
	SCONF_ERR_PART   = JSMN_ERROR_PART,
	/* index out of array bounds */
	SCONF_ERR_OOB    = SCONF_ERR_PART-1, 
	/* key not found */
	SCONF_ERR_NOTFND = SCONF_ERR_PART-2, 
	SCONF_SUCCESS    = JSMN_SUCCESS
	/* positive values indicate the # of tokens parsed */
} sconf_ret_t;

typedef jsmntok_t sconftok_t;

struct sconf {
	jsmn_parser jp;
	jsmntok_t *tokens;
	int ntokens;
	const char *js;
	unsigned int slen;

	unsigned int tok_start, tok_end;
	sconf_t type;
};

/*
 * Sequence:
 * sconf_init(sc...); sconf_parse(sc); query(sc...)
 */
void sconf_init(struct sconf *sc, const char *js, unsigned int slen, sconftok_t *tokens, unsigned int ntokens);
/* return value: >0 = # of tokens, <0 = sconf_ret_t */
int sconf_parse(struct sconf *sc);
char *sconf_err_str(sconf_ret_t retval);

/* Query functions: */
sconf_ret_t sconf_arr_str(struct sconf *sc, int idx, const char *str[], int *slen);
sconf_ret_t sconf_arr_int(struct sconf *sc, int idx, int *ret);
sconf_ret_t sconf_kv_idx(struct sconf *sc, int idx, const char *rkey[], int *klen, const char *rval[], int *vlen);
sconf_ret_t sconf_kv_lookup(struct sconf *sc, const char *key, int klen, const char *rval[], int *vlen);

struct sconf_kvs {
	const char *key, *val;
	int vlen;
};
int sconf_kv_populate(char *str, struct sconf_kvs *kvs, int nkvs);

#endif	/* SCONF_H */
