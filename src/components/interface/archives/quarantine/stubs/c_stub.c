
#include <cos_component.h>
#include <cos_debug.h>

/**
 * @Override SS_ipc_client_fault
 * 
 * Return zero from SS_ipc_client_fault to cause CSTUB_INVOKE to retry
 * in SS_ipc_marshal_args.
 * 
 * Currently this function should not make component invocations. They
 * don't seem to work.
 */
__attribute__((regparm(1))) int
SS_ipc_client_fault(cos_flt_off flt)
{
	int error_out = 0;
	switch(flt) {
	case COS_FLT_PGFLT:
		error_out = EFAULT;
		break;
	case COS_FLT_QUARANTINE:
		error_out = EFAULT;
		break;
	default:
		/* Any other fault is bad */
		assert(0);
	}
	return error_out;
}

