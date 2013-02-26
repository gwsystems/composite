#include "include/fpu.h"
#include "include/thread.h"

static int fpu_disabled = 1;
static struct thread *last_used_fpu;

int
fpu_save(struct thread *curr, struct thread *next)
{
	/* if next thread doesn't use fpu, then we just disable the fpu */
	if(!fpu_thread_uses_fp(next))
	{
		fpu_disable();
		return 0;
	}

    /*
     * next thread uses fpu
     * if no thread used fpu before, then we set next thread as the last_used_fpu
     */
	if(unlikely(last_used_fpu == NULL))
	{
		fpu_enable();
		last_used_fpu = next;
		return 0;
	}

	/*
     	 * next thread uses fpu
	 * last_used_fpu exists
	 * if last_used_fpu == next, then we simply re-enable the fpu for the thread
     */
	if(last_used_fpu == next)
	{
		fpu_enable();
		return 0;
	}

	/*
     * next thread uses fpu
	 * last_used_fpu exists
	 * if last_used_fpu != next, then we save current fpu states to last_used_fpu, restore next thread's fpu state
     */
	fpu_enable();
	fxsave(last_used_fpu);
	if(next->fpu.saved_fpu) fxrstor(next);
	last_used_fpu = next; 
	return 0;
}

inline int
fpu_thread_uses_fp(struct thread *thd)
{
	return thd->fpu.status;
}

int
fpu_is_disabled()
{
	//assert(fpu_read_cr0() & FPU_DISABLED ? fpu_disabled : !fpu_disabled);
	//return fpu_disabled;
	printk("fpu_is_disabled() executed :: fpu_disabled = %d cr0: %8x\n", fpu_disabled, fpu_read_cr0());
	return fpu_read_cr0() & FPU_DISABLED;
}

inline void
fxsave(struct thread *thd)
{
	asm volatile("fxsave %0" : "=m" (thd->fpu));
	thd->fpu.saved_fpu = 1;
}

inline void
fxrstor(struct thread *thd)
{
	asm volatile("fxrstor %0 " : : "m" (thd->fpu));
}

void
fpu_disable(void)
{
	unsigned int val;

	if(fpu_is_disabled()) return;

	asm volatile("mov %%cr0,%0" : "=r" (val));
	asm volatile("bts $3,%0" : "=r" (val));
	asm volatile("mov %0,%%cr0" : : "r" (val));

	fpu_disabled = 1;
	printk("fpu_disable() executed :: now fpu_disabled = %d cr0: %8x\n", fpu_disabled, fpu_read_cr0());
}

void
fpu_enable(void)
{
	unsigned int val;

	if (!fpu_is_disabled()) return;

	asm volatile("mov %%cr0,%0" : "=r" (val));
	asm volatile("btr $3,%0" : "=r" (val));
	asm volatile("mov %0,%%cr0" : : "r" (val));

	fpu_disabled = 0;
	printk("fpu_enable() executed :: now fpu_disabled = %d cr0: %8x\n", fpu_disabled, fpu_read_cr0());
}

inline unsigned int
fpu_read_cr0(void)
{
	unsigned int val;
	asm volatile("mov %%cr0,%0" : "=r" (val));
	return val;
}

inline struct thread*
fpu_get_last_used(void)
{
	return last_used_fpu;
}
