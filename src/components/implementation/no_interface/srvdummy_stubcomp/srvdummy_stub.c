/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

#include <hypercall.h>
#include <sinv_async.h>
#include <srv_dummy.h>
#include <srv_dummy_types.h>
#include <sched.h>
#include <schedinit.h>

struct sinv_async_info sinv_api;

/*
 * To demonstrate the use of library to make invocations directly
 * This may not be feasible in some cases, override the
 * WEAK SYMBOL sinv_server_entry as demonstrated in this file.
 * Doing so will let you have your own signatures for each function
 * than the standard INVOCATION signatures.
 */
/*
void
sinv_srv_dummy_init(struct sinv_async_info *a)
{
	int i;

	for (i = 0; i < SRV_DUMMY_MAX; i++) {
		vaddr_t api = srv_dummy_api(i);

		assert(api);
		switch(i) {
		case SRV_DUMMY_HELLO:
			sinv_server_api_init(a, i, (sinv_fn_t)api, NULL);
			break;
		case SRV_DUMMY_GOODBYE:
			sinv_server_api_init(a, i, NULL, (sinv_rets_fn_t)api);
			break;
		default: assert(0);
		}
	}
}
*/

/* override the weak symbol to implement my own invocations */
int
sinv_server_entry(struct sinv_async_info *s, struct sinv_call_req *req)
{
	int ret;

	assert(s);
	assert(req);
	assert(req->callno >= 0 && req->callno < SRV_DUMMY_MAX);

	switch(req->callno) {
	case SRV_DUMMY_HELLO:
	{
		ret = srv_dummy_hello(req->arg1, req->arg2, req->arg3);

		break;
	}
	case SRV_DUMMY_GOODBYE:
	{
		ret = srv_dummy_goodbye((int *)&(req->ret2), (int *)&(req->ret3), req->arg1, req->arg2, req->arg3);

		break;
	}
	default: assert(0);
	}

	return ret;
}

void
cos_init(void)
{
        spdid_t child;
        comp_flag_t childflag;
	unsigned long first = 1, init_done = 0;

        assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

	if (ps_cas(&first, 1, 0)) {
		sinv_server_init(&sinv_api, SRV_DUMMY_INSTANCE(SRV_DUMMY_ISTATIC));
		/* sinv_srv_dummy_init(&sinv_api); */

		ps_faa(&init_done, 1);
	} else {
		while (!ps_load(&init_done)) ;
	}

	schedinit_child();
	sinv_server_main_loop(&sinv_api);

	sched_thd_exit();
}
