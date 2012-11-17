#include "include/fpu.h"

inline void fsave(struct cos_fpu *fpu) {
	//asm volatile("fsave %0" : "=m" (thd->fpu));
	asm volatile("fsave %0" : "=m" (fpu));
}


inline void frstor(struct cos_fpu *fpu) {
	//asm volatile("frstor %0 " : : "m" (thd->fpu));
	asm volatile("frstor %0 " : : "m" (fpu));
}

inline void disable_fpu(void) {
	unsigned int val;
	asm volatile("mov %%cr0,%0" : "=r" (val));
	asm volatile("bts $3,%0" : "=r" (val));
	asm volatile("mov %0,%%cr0" : : "r" (val));
}

inline void enable_fpu(void) {
	unsigned int val;
	asm volatile("mov %%cr0,%0" : "=r" (val));
	asm volatile("btr $3,%0" : "=r" (val));
	asm volatile("mov %0,%%cr0" : : "r" (val));

}

unsigned int cos_read_cr0(void) {
	unsigned int val;
	asm volatile("mov %%cr0,%0" : "=r" (val));
	return val;
}

void print_cr0(void) {
	printk("cr0: %8x\n", read_cr0());
}
