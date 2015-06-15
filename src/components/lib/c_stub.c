
#include <cos_types.h>
#include <cstub.h>

__attribute__((regparm(1))) int
SS_ipc_client_marshal_args(struct usr_inv_cap *uc, int p0, int p1, int p2, int p3)
{
	int ret, fault;
	CSTUB_INVOKE(ret, fault, uc, 4, p0, p1, p2, p3);
	return ret;
}
