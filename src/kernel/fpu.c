#include "include/fpu.h"

struct thread *last_used_fpu;

void save_fpu(struct thread *thd) {
	if((last_used_fpu != NULL) && (last_used_fpu != thd)) {
		fsave(last_used_fpu);
		if(thd_saved_fpu(thd)) {
			if(fpu_is_disabled())
				enable_fpu();
			frstor(thd);
		}
	}
	last_used_fpu = thd;
}

int fpu_is_disabled() {
	if(cos_read_cr0() == 0x8005003b)
		return 1;
	else
		return 0;
}

int thd_saved_fpu(struct thread *thd) {
	if(thd->fpu.saved_fpu == 1)
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
