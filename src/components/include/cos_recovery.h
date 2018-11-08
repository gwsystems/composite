#ifndef COS_RECOVERY_H
#define COS_RECOVERY_H

#include "../interface/capmgr/capmgr.h"

static inline void
cos_recovery_ret_fn(void)
{
	unsigned long r1, r2, r3, op = COS_UPCALL_THD_CREATE;

	__asm__ __volatile__("" : "=a"(r1), "=S"(r2), "=D"(r3) : : );

	__asm__ __volatile__("jmp %%edx\n\t" : : "c"(op), "b"(r1), "a"(r2), "d"(r3) : );

	assert(0);
}

int
cos_thd_reset_entry(u32_t idx)
{
	int tmp, r1, r2, r3;

	r1 = capmgr_thd_reset_entry(cos_spd_id(), cos_thdid(), idx, &cos_recovery_ret_fn, &r2, &r3);
	assert(0);

	return r1;
}

#endif /* COS_RECOVERY_H */
