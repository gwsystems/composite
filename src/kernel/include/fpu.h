#ifndef FPU_H
#define FPU_H

/* TODO: Make this file architecture independent */

#include "per_cpu.h"
#define FPU_DISABLED_MASK 0x8
#define FXSR (1 << 24)
#define HAVE_SSE (1 << 25)

PERCPU_DECL(int, fpu_disabled);
PERCPU_EXTERN(fpu_disabled);

PERCPU_DECL(struct thread *, fpu_last_used);
PERCPU_EXTERN(fpu_last_used);

enum
{
	FPU_DISABLE = 0,
	FPU_ENABLE  = 1
};

/* fucntions called outside */
static inline int  fpu_init(void);
static inline int  fpu_disabled_exception_handler(void);
static inline void fpu_thread_init(struct thread *thd);
static inline int  fpu_switch(struct thread *next);
static inline void fpu_save(struct thread *);
static inline void fpu_restore(struct thread *);

/* packed functions for FPU operation */
static inline void fpu_enable(void);
static inline void fpu_disable(void);
static inline int  fpu_is_disabled(void);
static inline int  fpu_thread_uses_fp(struct thread *thd);

/* packed low level (assemmbly) functions */
static inline void          fxsave(struct thread *);
static inline void          fxrstor(struct thread *);
static inline unsigned long fpu_read_cr0(void);
static inline void          fpu_set(int);
static inline int           fpu_get_info(void);
static inline int           fpu_check_fxsr(void);
static inline int           fpu_check_sse(void);
#include "thd.h"

#if FPU_ENABLED
static inline int
fpu_get_info(void)
{
	static int cpu_info = 0;

	if (unlikely(cpu_info == 0)) {
		asm volatile("mov $1, %%eax\n\t"
		             "cpuid\n\t"
		             "movl %%edx, %0"
		             : "=m"(cpu_info)
		             :
		             : "eax", "ebx", "ecx", "edx");
	}
	/* printk("cpu %d cpuid_edx %x\n", get_cpuid(), cpu_info); */

	return cpu_info;
}

static inline int
fpu_check_fxsr(void)
{
	int cpu_info;
	int fxsr_status;

	cpu_info = fpu_get_info();
	/* fxsr is the 25th bit (start from bit 1) in EDX. So FXSR is 1<<24. */
	fxsr_status = ((cpu_info & FXSR) != 0) ? 1 : 0;

	return fxsr_status;
}

static inline int
fpu_check_sse(void)
{
	int cpu_info;
	int sse_status;

	cpu_info = fpu_get_info();
	/* sse is the 26th bit (start from bit 1) in EDX. So FXSR is 1<<25. */
	sse_status = ((cpu_info & HAVE_SSE) != 0) ? 1 : 0;

	return sse_status;
}

static inline int
fpu_init(void)
{
	fpu_set(FPU_DISABLE);
	*PERCPU_GET(fpu_disabled)  = 1;
	*PERCPU_GET(fpu_last_used) = NULL;

#if FPU_SUPPORT_FXSR > 0
	int fxsr = fpu_check_fxsr();
	int fsse = fpu_check_sse();

	if (fxsr == 0) {
		printk("Core %d: FPU doesn't support fxsave/fxrstor. Need to use fsave/frstr instead. Check "
		       "FPU_SUPPORT_FXSR in cos_config.\n",
		       get_cpuid());
		return -1;
	}
#endif

#if FPU_SUPPORT_SSE > 0
	if (fsse == 0) {
		printk("Core %d: FPU doesn't support sse. Check FPU_SUPPORT_SSE in cos_config.\n", get_cpuid());
		return -1;
	}
#endif

	return 0;
}

static inline void
fpu_disable(void)
{
	if (fpu_is_disabled()) return;

	fpu_set(FPU_DISABLE);
	*PERCPU_GET(fpu_disabled) = 1;

	return;
}

static inline int
fpu_is_disabled(void)
{
	int *disabled = PERCPU_GET(fpu_disabled);
	assert(fpu_read_cr0() & FPU_DISABLED_MASK ? *disabled : !*disabled);

	return *disabled;
}

static inline unsigned long
fpu_read_cr0(void)
{
	unsigned long val;
	asm volatile("mov %%cr0, %0" : "=r"(val));

	return val;
}

static inline void
fpu_set(int status)
{
	unsigned long val, cr0;

	/*
	 * Set CR0.TS = 1 to disable any use of x87 FPU/MMX/SSE/SSE2/SSE3/SSSE3/SSE4/AVX/AVX2 instruction, it will cause a #NM.
	 * The system uses this feature to detect if a user uses these instructions, if it doesn't use, don't execute related context
	 * switch (save & restore the registers above).
	 */
	cr0 = fpu_read_cr0();
	val = status ? (cr0 & ~FPU_DISABLED_MASK)
	             : (cr0 | FPU_DISABLED_MASK); // ENABLE(status == 1) : DISABLE(status == 0)
	asm volatile("mov %0, %%cr0" : : "r"(val));

	return;
}

static inline int
fpu_disabled_exception_handler(void)
{
	struct thread *curr_thd;
	curr_thd = cos_get_curr_thd();
	assert(curr_thd != NULL);

	/* If this thread enables fpu and there is still a #NM, tell the handler to deal with */
	if(!fpu_is_disabled()) return 0;

	curr_thd->fpu.status = 1;
	fpu_switch(curr_thd);

	return 1;
}

static inline void
fpu_thread_init(struct thread *thd)
{
	memset(&thd->fpu, 0, sizeof(struct cos_fpu));
	/* Have to set bit 63 of xcomp_bv to 1, or it will cause a #GP */
	thd->fpu.xcomp_bv |= ((u64_t)1 << 63);
	thd->fpu.cwd = 0x37f;
#if FPU_SUPPORT_SSE > 0
	/* 
	 * Mask all SSE exceptions, this will make processor ingore the exceptions
	 * and the user program has to deal with invalid SSE results. 
	 */
	thd->fpu.mxcsr = 0x1f80;
#endif
	return;
}

static inline int
fpu_switch(struct thread *next)
{
	struct thread **last_used = PERCPU_GET(fpu_last_used);
	/* if next thread doesn't use fpu, then we just disable the fpu */
	if (!fpu_thread_uses_fp(next)) {
		fpu_disable();
		return 0;
	}

	fpu_enable();
	/*
	 * next thread uses fpu
	 * if no thread used fpu before, then we set next thread as the fpu_last_used
	 */
	if (unlikely(*last_used == NULL)) goto store;

	/*
	 * next thread uses fpu
	 * fpu_last_used exists
	 * if fpu_last_used == next, then we simply re-enable the fpu for the thread
	 */
	if (*last_used == next) return 0;

	/*
	 * next thread uses fpu
	 * fpu_last_used exists
	 * if fpu_last_used != next, then we save current fpu states to fpu_last_used, restore next thread's fpu state
	 */
	fpu_save(*last_used);
store:
	fpu_restore(next);
	*last_used = next;

	return 0;
}

static inline void
fpu_enable(void)
{
	if (!fpu_is_disabled()) return;

	fpu_set(FPU_ENABLE);
	*PERCPU_GET(fpu_disabled) = 0;

	return;
}

static inline int
fpu_thread_uses_fp(struct thread *thd)
{
	return thd->fpu.status;
}

static inline void
fxsave(struct thread *thd)
{
#if FPU_SUPPORT_FXSR > 0
	asm volatile("fxsave %0" : "=m"(thd->fpu));
#else
	asm volatile("fsave %0" : "=m"(thd->fpu));
#endif
	return;
}

static inline void
fxrstor(struct thread *thd)
{
#if FPU_SUPPORT_FXSR > 0
	asm volatile("fxrstor %0" : : "m"(thd->fpu));
#else
	asm volatile("frstor %0" : : "m"(thd->fpu));
#endif
	return;
}

static inline void
xsaves(struct thread *thd)
{
#ifdef __x86_64__
	/* 0x7: XCR0_x87 | XCR0_x87 | XCR0_x87 */
	asm volatile("xsaves64 %0" : "=m"(thd->fpu): "a"(0x7), "d"(0):"memory");
#else
	asm volatile("xsaves %0" : "=m"(thd->fpu): "a"(0x7), "d"(0):"memory");
#endif
}

static inline void
xrestors(struct thread *thd)
{
#ifdef __x86_64__
	/* 0x7: XCR0_x87 | XCR0_x87 | XCR0_x87 */
	asm volatile("xrstors64 %0" : :"m"(thd->fpu), "a"(0x7), "d"(0):"memory");
#else
	asm volatile("xrstors %0" : :"m"(thd->fpu), "a"(0x7), "d"(0):"memory");
#endif
}

static inline void
fpu_save(struct thread *thd)
{
#if FPU_SUPPORT_XSAVES
	xsaves(thd);
#else
	fxsave(thd);
#endif
}

static inline void
fpu_restore(struct thread *thd)
{
#if FPU_SUPPORT_XSAVES
	xrestors(thd);
#else
	fxrstor(thd);
#endif
}

#else
/* if FPU_DISABLED is not defined, then we use these dummy functions */
static inline int
fpu_init(void)
{
	return 1;
}
static inline int
fpu_disabled_exception_handler(void)
{
	printk("COS KERNEL: ERROR: fpu is disabled in cos_config.h\n");	
	die("EXCEPTION: cannot handle Device not available exception\n");
	return 1;
}
static inline void
fpu_thread_init(struct thread *thd)
{
	return;
}
static inline int
fpu_switch(struct thread *next)
{
	return 0;
}
static inline void
fpu_enable(void)
{
	return;
}
static inline int
fpu_thread_uses_fp(struct thread *thd)
{
	return 0;
}
static inline void
fxsave(struct thread *thd)
{
	return;
}
static inline void
fxrstor(struct thread *thd)
{
	return;
}
static inline int
fpu_get_info(void)
{
	return 0;
}
static inline int
fpu_check_fxsr(void)
{
	return 0;
}
static inline int
fpu_check_sse(void)
{
	return 0;
}
static inline void
fpu_save(struct thread *)
{
	return;
}
static inline void
fpu_restore(struct thread *)
{
	return;
}
#endif

#endif
