#include "include/fpu.h"

struct thread *last_used_fpu;

void save_fpu(struct thread *thd) {
//	if(thd->fpu.status == 1) {
		if((last_used_fpu != NULL) && (last_used_fpu != thd)) {
			//printk("thd %d, fpu.swd is %10x\n", thd_get_id(last_used_fpu), last_used_fpu->fpu.swd);
			fsave(last_used_fpu);
			if(thd->fpu.swd != 0xffff0000) {
			//	printk("thd %d, fpu.swd is %10x\n", thd_get_id(last_used_fpu), last_used_fpu->fpu.swd);
			//	printk("thd %d, fpu.swd is %10x\n", thd_get_id(thd), thd->fpu.swd);
				if(cos_read_cr0() == 0x8005003b) {
					enable_fpu();
				}
				frstor(thd);
			}
			//else
			//	printk("thd %d, fpu.swd is %10x\n", thd_get_id(thd), thd->fpu.swd);
			
		}
		//else
		//	printk("%d\n", thd_get_id(last_used_fpu));
		last_used_fpu = thd;
//	}
/*
	else {
		disable_fpu();
	}
*/
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
