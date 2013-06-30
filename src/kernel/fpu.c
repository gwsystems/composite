#include "include/fpu.h"
#include "include/thread.h"

int
fpu_init()
{
        fpu_set(DISABLE);
        fpu_disabled = 1;
        last_used_fpu = NULL;

        printk("fpu_init on core %d\n", get_cpuid());

        return 0;
}

int
fpu_save(struct thread *curr, struct thread *next)
{
        /* if next thread doesn't use fpu, then we just disable the fpu */
        if (!fpu_thread_uses_fp(next)) {
                fpu_disable();
                return 0;
        }

        /*
         * next thread uses fpu
         * if no thread used fpu before, then we set next thread as the last_used_fpu
         */
        if(unlikely(last_used_fpu == NULL)) {
                fpu_enable();
                last_used_fpu = next;
                return 0;
        }

        /*
         * next thread uses fpu
         * last_used_fpu exists
         * if last_used_fpu == next, then we simply re-enable the fpu for the thread
         */
        if(last_used_fpu == next) {
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
        if (next->fpu.saved_fpu) fxrstor(next);
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
        assert(fpu_read_cr0() & FPU_DISABLED_MASK ? fpu_disabled : !fpu_disabled);

        return fpu_disabled;
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
        asm volatile("fxrstor %0" : : "m" (thd->fpu));
}


inline void
fpu_set(int status)
{
        unsigned long val, cr0;

        cr0 = fpu_read_cr0();
        val = status ?  (cr0 & ~FPU_DISABLED_MASK) : (cr0 | FPU_DISABLED_MASK); // ENABLE(status == 1) : DISABLE(status == 0)
        asm volatile("mov %0,%%cr0" : : "r" (val));

        return;
}

void
fpu_disable(void)
{
        if (fpu_is_disabled()) return;

        fpu_set(DISABLE);
        fpu_disabled = 1;

        return;
}

void
fpu_enable(void)
{
        if (!fpu_is_disabled()) return;

        fpu_set(ENABLE);
        fpu_disabled = 0;

        return;
}

inline unsigned long
fpu_read_cr0(void)
{
        unsigned long val;
        asm volatile("mov %%cr0,%0" : "=r" (val));

        return val;
}
