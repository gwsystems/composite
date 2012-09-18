#include "include/shared/consts.h"

static inline void fsave(struct cos_fpu *fpu)
{
	asm volatile("fnsave %[fx]; fwait"
			: [fx] "=" (fpu));
	
}

void frstor(struct cos_fpu *fpu)
{
	asm volatile("frstor %0",
			"m" (*fpu));
}
