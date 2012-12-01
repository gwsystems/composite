#include "include/fpu.h"

struct thread *last_used_fpu;

int save_fpu(struct thread *curr, struct thread *next) {
	// if next thread doesn't use fpu, then we just disable the fpu
	if(next->fpu.status == 0) {
		disable_fpu();
		return 0;
	}

	// next thread uses fpu
	// if no thread used fpu before, then we set next thread as the last_used_fpu
	if(last_used_fpu == NULL) {
		if(fpu_is_disabled()) enable_fpu();
		last_used_fpu = next;
		return 0;
	}

	// next thread uses fpu
	// last_used_fpu exists
	// if last_used_fpu == next, then we simply re-enable the fpu fot the thread
	if(last_used_fpu == next) {
		if(fpu_is_disabled()) enable_fpu();
		return 0;
	}

	// next thread uses fpu
	// last_used_fpu exists
	// if last_used_fpu != next, then we save current fpu states to last_used_fpu, restore next thread's fpu state
	if(fpu_is_disabled()) enable_fpu();
	fsave(last_used_fpu);
	if(next->fpu.saved_fpu) frstor(next);
	last_used_fpu = next; 
	return 0;
} 

int fpu_is_disabled() {
	if(cos_read_cr0() == 0x8005003b)
		return 1;
	else
		return 0;
}

inline void fsave(struct thread *thd) {
	asm volatile("fsave %0" : "=m" (thd->fpu));
	thd->fpu.saved_fpu = 1;
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
