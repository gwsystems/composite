/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cos_debug.h>
#include <srv_dummy.h>
#include <srv_dummy_types.h>
#include <sinv_async.h>

struct sinv_async_info sinv_info;

static int key_offset = 1;

int
srv_dummy_hello(int a, int b, int c)
{
	int ret = sinv_client_call(&sinv_info, SRV_DUMMY_HELLO, a, b, c);

        return ret;
}

int
srv_dummy_goodbye(int *r2, int *r3, int a, int b, int c)
{
        int r1 = sinv_client_rets_call(&sinv_info, SRV_DUMMY_GOODBYE, (word_t *)r2, (word_t *)r3, a, b, c);

        return r1;
}

void
srv_dummy_init(void)
{
	sinv_client_init(&sinv_info, SRV_DUMMY_INSTANCE(SRV_DUMMY_ISTATIC));
}

int
srv_dummy_thdinit(thdid_t tid, int is_aep)
{
	int ret;
	int off = key_offset;

	ps_faa((unsigned long *)&key_offset, 1);
	ret = sinv_client_thread_init(&sinv_info, tid, is_aep ? 0 : SRV_DUMMY_RKEY(SRV_DUMMY_ISTATIC, off), SRV_DUMMY_SKEY(SRV_DUMMY_ISTATIC, off));
        assert(ret == 0);

	return ret;
}
