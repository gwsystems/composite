#include "include/fpu.h"

inline void fsave(struct thread *thd)
{
	asm volatile("fsave %0" : "=m" (thd->fpu));
}


inline void frstor(struct thread *thd)
{
	asm volatile("frstor %0 " : : "m" (thd->fpu));
}

inline void set_ts(void)
{
	unsigned int val;
	asm volatile("mov %%cr0,%0" : "=r" (val));
	asm volatile("bts $3,%0" : "=r" (val));
	asm volatile("mov %0,%%cr0" : : "r" (val));
}

inline void clr_ts(void)
{
	//asm volatile("clts");
	unsigned int val;
	asm volatile("mov %%cr0,%0" : "=r" (val));
	asm volatile("btr $3,%0" : "=r" (val));
	asm volatile("mov %0,%%cr0" : : "r" (val));

}

inline unsigned int cos_read_cr0(void)
{
	unsigned int val;
	asm volatile("mov %%cr0,%0" : "=r" (val));
	return val;
}

inline void set_em(void) {
	unsigned int val;
	asm volatile("mov %%cr0,%0" : "=r" (val));
	asm volatile("bts $2,%0" : "=r" (val));
	asm volatile("mov %0,%%cr0" : : "r" (val));
}

inline void clr_em(void) {
	unsigned int val;
	asm volatile("mov %%cr0,%0" : "=r" (val));
	asm volatile("btr $2,%0" : "=r" (val));
	asm volatile("mov %0,%%cr0" : : "r" (val));

}
