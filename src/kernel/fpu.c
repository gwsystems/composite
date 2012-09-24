#include "include/fpu.h"

void fsave(struct cos_fpu *fpu)
{
	asm volatile("fnsave %0" : "=m" (fpu));
	
}

void frstor(struct cos_fpu fpu)
{
	asm volatile("frstor %0" : : "m" (fpu));
}
