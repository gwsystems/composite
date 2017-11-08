
#include <cos_component.h>
#include <cos_types.h>
#include <cstub.h>
#include <llprint.h>

/* Avoid creating any dependencies in the default stub code */
#define assert(x)                           \
	do {                                \
		volatile int y;             \
		if (!(x)) y = *(int *)NULL; \
	} while (0)

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
	int ret;
	
	ret = cos_sinv(uc->cap_no, 0,0,0,0);		
	
	return ret;
}
#undef assert
