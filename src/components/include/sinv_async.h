#ifndef SINV_ASYNC_H
#define SINV_ASYNC_H

#include <cos_types.h>
#include <cos_kernel_api.h>

#define SINV_NUM_MAX 16
#define SINV_INIT_NPAGES 1
#define SINV_REQ_NPAGES  1

/* TODO: change this to enumeration of invocation functions */
typedef int sinv_num_t;

typedef int (*sinv_fn_t)(word_t a, word_t b, word_t c);
typedef int (*sinv_rets_fn_t)(word_t *r2, word_t *r3, word_t a, word_t b, word_t c);

struct sinv_call_req {
	sinv_num_t callno;
	word_t arg1, arg2, arg3; /* by client */
	word_t ret2, ret3; /* by server */
};

struct sinv_thdcrt_req {
	spdid_t clspdid;

	cos_channelkey_t rkey;
	cos_channelkey_t skey;
};

/* SERVER API */
/* @shm_key - server maps shm to service thread creation requests. (polling only & 1 req at a time) */
void sinv_server_init(cos_channelkey_t shm_key);
/*
 * if the inv function returns multiple return values, set @fnr to appropriate fn and @fn == NULL
 * if the inv function returns only a single value, set @fn to appropriate fn and @fnr == NULL
 *
 * return: 0 on success. -ve on error.
 */
int sinv_server_api_init(sinv_num_t num, sinv_fn_t fn, sinv_rets_fn_t fnr);
int sinv_server_main_loop(void) __attribute__((noreturn));

/* CLIENT API */
/* @shm_key - client to make "thread" creation requests for server side. (polling only & 1 req at a time) */
void sinv_client_init(cos_channelkey_t shm_key);
/*
 * This API makes a cross-core request to create AEP thread on the server side..
 * (Does to synchronously to create "asndcap" for client requests.
 * @rcvkey == 0, thread is AEP thread, cannot "cos_asnd" on return.
 * @shm_snd_key, key used for shared memory (created by the client) and for "cos_asnd" from client to the server.
 *               therefore, the server thread should create AEP using this key!
*/
int sinv_client_thread_init(thdid_t tid, cos_channelkey_t rcvkey, cos_channelkey_t shm_snd_key);
int sinv_client_call(sinv_num_t, word_t a, word_t b, word_t c);
int sinv_client_rets_call(sinv_num_t, word_t *r2, word_t *r3, word_t a, word_t b, word_t c);

#endif /* SINV_ASYNC_H */
