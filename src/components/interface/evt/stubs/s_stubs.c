#include <evt.h>
#include <cos_stubs.h>

COS_SERVER_3RET_STUB(int, __evt_get)
{
	evt_res_type_t type;
	evt_res_data_t data;
	int            ret;

	ret = __evt_get(p0, p1, &type, &data);
	*r1 = type;
	*r2 = data;

	return ret;
}
