#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "sconf.h"

void 
tokens_print(struct sconf *sc)
{
	int i, jslen;
	jsmntok_t *ts = sc->tokens;

	jslen = strlen(sc->js)+1;
	for (i = 0 ; i < sc->ntokens && i < sc->jp.toknext ; i++, ts++) {
		char s[256];
		int len = ts->end - ts->start;

		assert(ts->end < jslen);
		memcpy(s, &sc->js[ts->start], len);
		s[len] = '\0';
		
		printf("type %d, contents %s, strlen %d, sz %d\n", 
		       ts->type, s, len, ts->size);
	}
}

int
array_test(void)
{
	struct sconf sc;
	sconftok_t tokens[10];
	char *str = "[10, 12, 14]";
	const char *sret;
	int ret, slen;
	sconf_ret_t r;
	
	sconf_init(&sc, str, strlen(str)+1, tokens, 10);
	ret = sconf_parse(&sc);
	assert(ret == 3);
	//tokens_print(&sc);
	r = sconf_arr_int(&sc, 0, &ret);
	assert(r == 0);
	assert(ret == 10);
	r = sconf_arr_int(&sc, 1, &ret);
	assert(r == 0);
	assert(ret == 12);
	r = sconf_arr_int(&sc, 2, &ret);
	assert(r == 0);
	assert(ret == 14);
	assert(sconf_arr_int(&sc, 3, &ret) == SCONF_ERR_OOB);

	r = sconf_arr_str(&sc, 0, &sret, &slen);
	assert(r == 0);
	assert(sret == str+1 && slen == 2);
	r = sconf_arr_str(&sc, 1, &sret, &slen);
	assert(r == 0);
	assert(sret == str+5 && slen == 2);
	r = sconf_arr_str(&sc, 2, &sret, &slen);
	assert(r == 0);
	assert(sret == str+9 && slen == 2);
	r = sconf_arr_str(&sc, 3, &sret, &slen);
	assert(r == SCONF_ERR_OOB);

	return 0;
}

int
kv_test(char *str)
{
	struct sconf sc;
	sconftok_t tokens[10];
	const char *rval, *rkey;
	int ret, vlen, klen;
	sconf_ret_t r;
	
	sconf_init(&sc, str, strlen(str)+1, tokens, 10);
	ret = sconf_parse(&sc);
	assert(ret == 7 || ret == 6);
	//tokens_print(&sc);
	r = sconf_kv_idx(&sc, 0, &rkey, &klen, &rval, &vlen);
	assert(r == 0);
	assert(rkey == str+1 && klen == 3 && rval == str+5 && vlen == 3);
	r = sconf_kv_idx(&sc, 2, &rkey, &klen, &rval, &vlen);
	assert(r == 0);
	assert(rkey == str+17 && klen == 1 && rval == str+19 && vlen == 10);

	r = sconf_kv_lookup(&sc, "10", 2, &rval, &vlen);
	assert(r == 0);
	assert(vlen == 2 && !strncmp("12", rval, 2));
	r = sconf_kv_lookup(&sc, "notthere", 8, &rval, &vlen);
	assert(r == SCONF_ERR_NOTFND);

	return 0;
}

int
kv_test2(char *str)
{
	struct sconf_kvs kvs[3] = {{.key = "hello", .val = NULL}, 
				   {.key = "p",     .val = NULL},
				   {.key = "wtf",   .val = NULL}};
	if (sconf_kv_populate(str, kvs, 3)) return -1;
	assert(!strncmp(kvs[0].val, "world", 5));
	assert(!strncmp(kvs[1].val, "what in the hell", 16));
	assert(kvs[2].val == NULL);

	return 0;
}

int
main(void)
{
	array_test();
	kv_test("{foo:bar, 10:12, a:helloworld}");
	kv_test(" foo:bar \n10:12 \na:helloworld ");
	kv_test2("hello:world, p:\"what in the hell\", third: nothing");

	return 0;
}
