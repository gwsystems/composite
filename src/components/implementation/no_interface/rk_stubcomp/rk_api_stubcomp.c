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

static struct sinv_async_info sinv_api;
static int instance_key = 0;

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

void
cos_init(void)
{
        spdid_t child;
        comp_flag_t childflag;
	unsigned long first = 1, init_done = 0;

        assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

	PRINTC("RK STUBCOMP INIT!\n");
	if (ps_cas(&first, 1, 0)) {
		instance_key = rk_args_instance();
		assert(instance_key > 0);

		/* because RK is non-preemptive. request notif should be sent to initthd or scheduling thread */
		sinv_server_init_sndkey(&sinv_api, RK_CLIENT(instance_key), RK_INITRCV_KEY);
		sinv_rk_init(&sinv_api);

		ps_faa(&init_done, 1);
	} else {
		/* only run on one core! */
		assert(0);

		while (!ps_load(&init_done)) ;
	}

	PRINTC("ENTERING MAIN LOOP\n");
	/* serving only 1 cFE thread! */
	sinv_server_main_nrequests(&sinv_api, 1);

	/* remove from scheduling queue after that */
	sched_thd_exit();
}
