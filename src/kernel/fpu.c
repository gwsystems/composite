#include "include/fpu.h"

struct thread *last_used_fpu;

void fpu_op(struct thread *thd) {
	if(last_used_fpu != NULL) {
		printk("1.last_used_fpu id is: %d\n", thd_get_id(last_used_fpu));
		fsave(last_used_fpu);
		if(thd->fpu.swd != 0) {
		frstor(thd);
		}
	}
	last_used_fpu = thd;
		printk("2.last_used_fpu id is: %d\n", thd_get_id(last_used_fpu));
}

inline void fsave(struct thread *thd) {
	asm volatile("fsave %0" : "=m" (thd->fpu));
}


inline void frstor(struct thread *thd) {
	asm volatile("frstor %0 " : : "m" (thd->fpu));
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
