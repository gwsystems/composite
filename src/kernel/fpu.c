#include "include/fpu.h"

void fsave(struct thread *thd)
{
	asm volatile("fnsave %0; fwait " : "=m" (thd->fpu));
}


void frstor(struct thread *thd)
{
	asm volatile("frstor %0 " : : "m" (thd->fpu));
}

void finit()
{
	asm volatile("finit;");
}
