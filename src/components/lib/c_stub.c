#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cstub.h>

__attribute__((regparm(1))) int
__cosrt_c_cosrtdefault(struct usr_inv_cap *uc, long p0, long p1, long p2, long p3)
{
	return cos_sinv(uc->cap_no, 0, p0, p1, p2);
}
