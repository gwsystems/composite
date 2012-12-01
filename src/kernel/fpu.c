#include "include/fpu.h"

struct thread *last_used_fpu;

void save_fpu(struct thread *curr, struct thread *next) {
	if(next->fpu.status) {
		if(last_used_fpu != NULL) {
			if(last_used_fpu == next) 
				enable_fpu();
			else {
				if(fpu_is_disabled())
					enable_fpu();

				fsave(last_used_fpu);
				if(next->fpu.saved_fpu) {
					frstor(next);
				}    
				last_used_fpu = next; 
			}    
		} else {
			enable_fpu();
			last_used_fpu = next; 
		}    
	} else {
		if(curr->fpu.status)
			last_used_fpu = curr;
		disable_fpu();
	} 
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
