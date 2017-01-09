#include <cbuf_mgr.h>
#include <cstub.h>
#include <print.h>

CSTUB_FN(int, cbuf_create)(struct usr_inv_cap *uc,
                           spdid_t spdid, unsigned long size, int cbid)
{
	int ret;
	long fault = 0;

	do {
		fault = 0;
		printc("invoking create stub\n");
		CSTUB_INVOKE(ret, fault, uc, 3, spdid, size, cbid);
		printc("finished invoking with fault %d ret %d\n", fault, ret);
	} while (fault != 1);

	return ret;
}


CSTUB_FN(int, cbuf_map_collect)(struct usr_inv_cap *uc,
                                spdid_t spdid)
{
	int ret;
	long fault = 0;

	do {	
		fault = 0;
		printc("Invoking map collect stub\n");
		CSTUB_INVOKE(ret, fault, uc, 1, spdid);
		printc("Finished invoking with fault %d ret %d\n", fault, ret);
	} while (fault != 1);

	return ret;
}

