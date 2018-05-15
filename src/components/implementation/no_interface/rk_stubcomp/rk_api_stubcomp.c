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
#include <rk_inv.h>
#include <rk.h>
#include <rk_types.h>
#include <rk_calls.h>
#include <sched.h>

struct sinv_async_info sinv_api;

void
sinv_rk_init(struct sinv_async_info *a)
{
	int i;

	for (i = 0; i < RK_API_MAX; i++) {
		vaddr_t api = rk_api(i);

		assert(api);
		sinv_server_api_init(a, i, (sinv_fn_t)api, NULL);
	}
}

#define RK_CLIENT_INSTANCE 1

void
cos_init(void)
{
        spdid_t child;
        comp_flag_t childflag;
	unsigned long first = 1, init_done = 0;

        assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

	if (ps_cas(&first, 1, 0)) {
		sinv_server_init(&sinv_api, RK_CLIENT(RK_CLIENT_INSTANCE));
		sinv_rk_init(&sinv_api);

		ps_faa(&init_done, 1);
	} else {
		while (!ps_load(&init_done)) ;
	}

	sinv_server_main_loop(&sinv_api);

	sched_thd_exit();
}
