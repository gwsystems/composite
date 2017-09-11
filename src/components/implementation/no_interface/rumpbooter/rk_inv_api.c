#include "vk_types.h"
#include "rk_inv_api.h"
#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

int
rk_inv_op1(void)
{
	return cos_sinv(VM_CAPTBL_SELF_RK_SINV_BASE, RK_INV_OP1, 0, 0, 0);
}

void
rk_inv_op2(int shmid)
{
	cos_sinv(VM_CAPTBL_SELF_RK_SINV_BASE, RK_INV_OP2, shmid, 0, 0);
}

