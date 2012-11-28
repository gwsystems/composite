#include "include/fpu.h"

struct thread *last_used_fpu;

void save_fpu(struct thread *thd) {
	if(cos_read_cr0() == 0x8005003b)
		enable_fpu();
	if(last_used_fpu != NULL) {
		if(last_used_fpu != thd) {
			fsave(last_used_fpu);
			if(thd->fpu.swd != NULL) {
				frstor(thd);
			}
		}
	}
	if(last_used_fpu != thd)
		last_used_fpu = thd;
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
