#ifndef FPU_H
#define FPU_H

#include "thread.h"

#define ENABLE  1
#define DISABLE 0
#define FPU_DISABLED_MASK 0x8

extern int fpu_disabled;
extern struct thread *last_used_fpu;

/* fucntions called outside */
static inline int fpu_init(void);
static inline int fpu_save(struct thread *curr, struct thread *next);

/* packed functions for FPU operation */
static inline void fpu_enable(void);
static inline void fpu_disable(void);
static inline int fpu_is_disabled(void);
static inline int fpu_thread_uses_fp(struct thread *thd);

/* packed low level (assemmbly) functions */
static inline void fxsave(struct thread*);
static inline void fxrstor(struct thread*);
static inline unsigned long fpu_read_cr0(void);
static inline void fpu_set(int);

static inline int
fpu_init(void)
{
        fpu_set(DISABLE);
        fpu_disabled = 1;
        last_used_fpu = NULL;

        printk("fpu_init on core %d\n", get_cpuid());

        return 0;
}

static inline int
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

static inline void
fpu_enable(void)
{
        if (!fpu_is_disabled()) return;

        fpu_set(ENABLE);
        fpu_disabled = 0;

        return;
}

static inline void
fpu_disable(void)
{
        if (fpu_is_disabled()) return;

        fpu_set(DISABLE);
        fpu_disabled = 1;

        return;
}

static inline int
fpu_is_disabled(void)
{
        assert(fpu_read_cr0() & FPU_DISABLED_MASK ? fpu_disabled : !fpu_disabled);

        return fpu_disabled;
}

static inline int
fpu_thread_uses_fp(struct thread *thd)
{
        return thd->fpu.status;
}

static inline unsigned long
fpu_read_cr0(void)
{
        unsigned long val;
        asm volatile("mov %%cr0,%0" : "=r" (val));

        return val;
}

static inline void
fpu_set(int status)
{
        unsigned long val, cr0;

        cr0 = fpu_read_cr0();
        val = status ?  (cr0 & ~FPU_DISABLED_MASK) : (cr0 | FPU_DISABLED_MASK); // ENABLE(status == 1) : DISABLE(status == 0)
        asm volatile("mov %0,%%cr0" : : "r" (val));

        return;
}

static inline void
fxsave(struct thread *thd)
{
        asm volatile("fxsave %0" : "=m" (thd->fpu));
        thd->fpu.saved_fpu = 1;
}

static inline void
fxrstor(struct thread *thd)
{
        asm volatile("fxrstor %0" : : "m" (thd->fpu));
}

#endif
