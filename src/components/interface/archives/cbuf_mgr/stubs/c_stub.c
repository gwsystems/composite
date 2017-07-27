#include <cbuf_mgr.h>
#include <cstub.h>
#include <print.h>
/*
CSTUB_FN(int, cbuf_create)(struct usr_inv_cap *uc,
                           spdid_t spdid, unsigned long size, int cbid)
{
	int ret;
	long fault = 0;

	do {
		fault = 0;
		CSTUB_INVOKE(ret, fault, uc, 3, spdid, size, cbid);
	} while (fault != 1);

	return ret;
}
*/

CSTUB_FN(int, cbuf_map_collect)(struct usr_inv_cap *uc,
                                spdid_t spdid)
{
	int ret;
	long fault = 0;

	do {	
		fault = 0;
		CSTUB_INVOKE(ret, fault, uc, 1, spdid);
	} while (fault != 1);

	return ret;
}

CSTUB_FN(int, cbuf_delete)(struct usr_inv_cap *uc,
                           spdid_t spdid, unsigned int cb)
{
	int ret;
	long fault = 0;

	do {	
		fault = 0;
		CSTUB_INVOKE(ret, fault, uc, 2, spdid, cb);
	} while (fault != 1);

	return ret;
}
