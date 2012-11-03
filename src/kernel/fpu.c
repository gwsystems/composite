#include "include/fpu.h"

inline void fsave(struct thread *thd)
{
	asm volatile("fsave %0" : "=m" (thd->fpu));
}


inline void frstor(struct thread *thd)
{
	asm volatile("frstor %0 " : : "m" (thd->fpu));
}
/*
void set_ts(void)
{
	unsigned int val;
	asm volatile("mov %%cr0,%0" : "=r" (val));
	//printk("%10x\n", val);
	val = val | 0x00000008;
	//printk("%10x\n", val);
	asm volatile("mov %0,%%cr0" : : "r" (val));
}

void clr_ts(void)
{
	asm volatile("clts");
}

unsigned int cos_read_cr0(void)
{
	unsigned int val;
	asm volatile("mov %%cr0,%0" : "=r" (val));
	return val;
}
*/
