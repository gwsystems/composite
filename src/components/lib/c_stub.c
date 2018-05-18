#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cstub.h>

/* Avoid creating any dependencies in the default stub code */
#ifndef assert
#define assert(x)                           \
	do {                                \
		volatile int y;             \
		if (!(x)) y = *(int *)NULL; \
	} while (0)
#endif

/* Return zero from SS_ipc_client_fault to cause CSTUB_INVOKE to retry */
__attribute__((weak, regparm(1))) int
SS_ipc_client_fault(cos_flt_off flt)
{
	int error_out = 0;

	switch (flt) {
	case COS_FLT_PGFLT:
		error_out = -EFAULT;
		break;
	default:
		/* Any other fault is bad */
		assert(0);
	}

	return error_out;
}

__attribute__((regparm(1))) int
SS_ipc_client_marshal_args(struct usr_inv_cap *uc, long p0, long p1, long p2, long p3)
{
	return cos_sinv(uc->cap_no, p0, p1, p2, p3);
}

__attribute__((regparm(1))) int
SS_ipc_client_marshal_args_rets(struct usr_inv_cap *uc, long *r2, long *r3, long p0, long p1, long p2, long p3)
{
	return cos_sinv_rets(uc->cap_no, p0, p1, p2, p3, (word_t *)r2, (word_t *)r3);
}
