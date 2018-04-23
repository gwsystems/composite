/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#ifndef SINV_ASYNC_H
#define SINV_ASYNC_H

#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

#define SINV_NUM_MAX 16
#define SINV_INIT_NPAGES 1
#define SINV_REQ_NPAGES  1

#define SINV_REQ_SET   1
#define SINV_REQ_RESET 0

/*
 * Client sets SINV_REQ_SET at this address after writing to SINV_REQ_ADDR with the request.
 * Server polls for SINV_REQ_SET at this address and resets it to SINV_REQ_RESET
 * after processing the request and saving return value at SINV_RET_ADDR.
 */
#define SINV_POLL_ADDR(addr) ((unsigned long *)(addr))
/* Server saves the integer return value at this address */
#define SINV_RET_ADDR(addr) ((unsigned long *)(addr) + 1)
/*
 * Client writes struct sinv_call_req here at this address.
 * If client requests for multiple return values, server updates sinv_call_req.ret2 and sinv_call_req.ret3
 */
#define SINV_REQ_ADDR(addr) ((unsigned long *)(addr) + 2)

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

struct sinv_thdinfo {
	cos_channelkey_t rkey;
	cos_channelkey_t skey;

	spdid_t clientid;
	vaddr_t shmaddr;
	asndcap_t sndcap;
	arcvcap_t rcvcap;
} CACHE_ALIGNED;

struct sinv_client {
	struct sinv_thdinfo cthds[MAX_NUM_THREADS];
};

typedef enum {
	SINV_FN = 0,
	SINV_RETS_FN = 1,
} sinv_fn_type_t;

struct sinv_server {
	struct sinv_thdinfo sthds[MAX_NUM_THREADS];
	struct sinv_fns {
		sinv_fn_type_t type;
		union {
			sinv_fn_t      sfn;
			sinv_rets_fn_t sfnr;
		};
	} f[SINV_NUM_MAX];
};

struct sinv_async_info {
	cos_channelkey_t init_key;
	vaddr_t          init_shmaddr;

	union {
		struct sinv_client cdata;
		struct sinv_server sdata;
	};
};

/* SERVER API */
/* @shm_key - server maps shm to service thread creation requests. (polling only & 1 req at a time) */
void sinv_server_init(struct sinv_async_info *s, cos_channelkey_t shm_key);
/*
 * if the inv function returns multiple return values, set @fnr to appropriate fn and @fn == NULL
 * if the inv function returns only a single value, set @fn to appropriate fn and @fnr == NULL
 *
 * return: 0 on success. -ve on error.
 */
int sinv_server_api_init(struct sinv_async_info *s, sinv_num_t num, sinv_fn_t fn, sinv_rets_fn_t fnr);
int sinv_server_main_loop(struct sinv_async_info *s) __attribute__((noreturn));

/* CLIENT API */
/* @shm_key - client to make "thread" creation requests for server side. (polling only & 1 req at a time) */
void sinv_client_init(struct sinv_async_info *s, cos_channelkey_t shm_key);
/*
 * This API makes a cross-core request to create AEP thread on the server side..
 * (Does to synchronously to create "asndcap" for client requests.
 * @rcvkey == 0, thread is AEP thread, cannot "cos_asnd" on return.
 * @shm_snd_key, key used for shared memory (created by the client) and for "cos_asnd" from client to the server.
 *               therefore, the server thread should create AEP using this key!
*/
int sinv_client_thread_init(struct sinv_async_info *s, thdid_t tid, cos_channelkey_t rcvkey, cos_channelkey_t shm_snd_key);
int sinv_client_call(struct sinv_async_info *s, sinv_num_t, word_t a, word_t b, word_t c);
int sinv_client_rets_call(struct sinv_async_info *s, sinv_num_t, word_t *r2, word_t *r3, word_t a, word_t b, word_t c);

/* Asynchronous communication API */
typedef sinv_num_t acom_type_t;
/* @shm_key - client to make "thread" creation requests for server side. (polling only & 1 req at a time) */
void acom_client_init(struct sinv_async_info *s, cos_channelkey_t shm_key);
int acom_client_thread_init(struct sinv_async_info *s, thdid_t, arcvcap_t, cos_channelkey_t rkey, cos_channelkey_t skey);
int acom_client_request(struct sinv_async_info *s, acom_type_t t, word_t a, word_t b, word_t c, tcap_res_t budget, tcap_prio_t prio);

#endif /* SINV_ASYNC_H */
