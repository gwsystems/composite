#ifndef LLBOOTER_INV_H
#define LLBOOTER_INV_H

#include <cos_kernel_api.h>

/* Parameters for specifying resource requests from booter */
typedef enum {
	INIT_DONE,
	BOOT_HYP_PGTBL_CAP,
	BOOT_HYP_SINV_CAP,
	BOOT_HYP_CAP_FRONTIER,
	BOOT_HYP_NUM_COMPS,
	BOOT_HYP_COMP_CAP,
} boot_hyp_op_t;

/*
 * Calls down to the booter to access necessary resources that are needed for
 * higher level resource managers
 */
static long
cos_hypervisor_hypercall(boot_hyp_op_t op, void *arg1, void *arg2, void *arg3)
{
	/*
	 * FIXME:
	 * There is no mechanism for checking to see if a comp should be permitted to
	 * do some of these operations
	 *
	 * TODO:
	 * Set each argument to a variable with a real name
	 */

	long ret;
	capid_t cap_index;
	assert(op >= 0);

	/* Depending on the operation we will need to allocate our own a cap space */
	switch(op) {
	case INIT_DONE:
		cos_sinv(BOOT_CAPTBL_SINV_CAP, op, 0, 0, 0);
		break;
	case BOOT_HYP_PGTBL_CAP:
		cap_index = cos_capid_bump_alloc((struct cos_compinfo *)arg3, CAP_PGTBL);
		assert(cap_index > 0);
		ret = cos_sinv(BOOT_CAPTBL_SINV_CAP, op, (int)arg1, (int)arg2, (int)cap_index);
		if (!ret) ret = cap_index;
		break;
	case BOOT_HYP_SINV_CAP:
		/* TODO */
		assert(0);
		break;
	case BOOT_HYP_CAP_FRONTIER:
		/* Get the offset of the cap frontier as known by the hypervisor */
		ret = cos_sinv(BOOT_CAPTBL_SINV_CAP, op, (int)arg1, (int)arg2, (int)arg3);
		break;
	case BOOT_HYP_NUM_COMPS:
		/* Ask the hypervisor for the number of components on the system */
		ret = cos_sinv(BOOT_CAPTBL_SINV_CAP, op, (int)arg1, (int)arg2, (int)arg3);
		break;
	case BOOT_HYP_COMP_CAP:
		cap_index = cos_capid_bump_alloc((struct cos_compinfo *)arg3, CAP_COMP);
		assert(cap_index > 0);
		ret = cos_sinv(BOOT_CAPTBL_SINV_CAP, op, (int)arg1, (int)arg2, (int)cap_index);
		if (!ret) ret = cap_index;
		break;
	default:
		assert(0);
	}

	return ret;
}

#endif /* LLBOOTER_INV_H */
