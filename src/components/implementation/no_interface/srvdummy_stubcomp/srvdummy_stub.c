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

void
cos_init(void)
{
        spdid_t child;
        comp_flag_t childflag;
	unsigned long first = 1, init_done = 0;

        assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

	if (ps_cas(&first, 1, 0)) {
		sinv_server_init(&sinv_api, SRV_DUMMY_INSTANCE(SRV_DUMMY_ISTATIC));
		sinv_srv_dummy_init(&sinv_api);

		ps_faa(&init_done, 1);
	} else {
		while (!ps_load(&init_done)) ;
	}

	schedinit_child();
	sinv_server_main_loop(&sinv_api);

	sched_thd_exit();
}
