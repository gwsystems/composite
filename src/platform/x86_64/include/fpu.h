#pragma once

//#include <chal.h>
#include <cos_regs.h>
#include <string.h> // memset

#define FPU_SUPPORT_SSE 1
#define FPU_SUPPORT_FXSR 1 /* >0 : CPU supports FXSR. */
#define FPU_SUPPORT_XSAVE 1
#define FPU_SUPPORT_XSAVEOPT 1
#define FPU_SUPPORT_XSAVEC 1
#define FPU_SUPPORT_XSAVES 1

#define FPU_DISABLED_MASK 0x8
#define FXSR (1 << 24)
#define HAVE_SSE (1 << 25)

/* /\* fucntions called outside *\/ */
/* static inline int  fpu_init(void); */
/* static inline int  fpu_disabled_exception_handler(void); */
/* static inline void fpu_thread_init(struct fpu_regs *thd); */
/* static inline int  fpu_switch(struct thread *next); */
/* static inline void fpu_save(struct thread *); */
/* static inline void fpu_restore(struct thread *); */

/* /\* packed functions for FPU operation *\/ */
/* static inline void fpu_enable(void); */
/* static inline void fpu_disable(void); */
/* static inline int  fpu_is_disabled(void); */
/* static inline int  fpu_thread_uses_fp(struct thread *thd); */

/* packed low level (assemmbly) functions */
/* static inline void          fxsave(struct fpu_regs *); */
/* static inline void          fxrstor(struct fpu_regs *); */
/* static inline unsigned long fpu_read_cr0(void); */
/* static inline void          fpu_set(int); */
/* static inline int           fpu_get_info(void); */
/* static inline int           fpu_check_fxsr(void); */
/* static inline int           fpu_check_sse(void); */

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

/* static inline unsigned long */
/* fpu_read_cr0(void) */
/* { */
/* 	unsigned long val; */
/* 	asm volatile("mov %%cr0, %0" : "=r"(val)); */

/* 	return val; */
/* } */

/*
 * Enable the floating point logic within the CPU.
 */
static inline int
fpu_init(void)
{
	unsigned long cr0;

	/*
	 * Set CR0.TS = 1 to disable any use of x87 FPU/MMX/SSE/SSE2/SSE3/SSSE3/SSE4/AVX/AVX2 instruction, it will cause a #NM.
	 * The system uses this feature to detect if a user uses these instructions, if it doesn't use, don't execute related context
	 * switch (save & restore the registers above).
	 */
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	asm volatile("mov %0, %%cr0" : : "r"(cr0 & ~FPU_DISABLED_MASK));

#if FPU_SUPPORT_FXSR > 0
	int fxsr = fpu_check_fxsr();

	if (fxsr == 0) return -1;
#endif

#if FPU_SUPPORT_SSE > 0
	int fsse = fpu_check_sse();

	if (fsse == 0) return -1;
#endif

	return 0;
}

/* static inline void */
/* fpu_disable(void) */
/* { */
/* 	if (fpu_is_disabled()) return; */

/* 	fpu_set(FPU_DISABLE); */
/* 	*PERCPU_GET(fpu_disabled) = 1; */

/* 	return; */
/* } */

/* static inline int */
/* fpu_is_disabled(void) */
/* { */
/* 	int *disabled = PERCPU_GET(fpu_disabled); */
/* 	assert(fpu_read_cr0() & FPU_DISABLED_MASK ? *disabled : !*disabled); */

/* 	return *disabled; */
/* } */

/* static inline void */
/* fpu_set(int status) */
/* { */
/* 	unsigned long val, cr0; */

/* 	/\* */
/* 	 * Set CR0.TS = 1 to disable any use of x87 FPU/MMX/SSE/SSE2/SSE3/SSSE3/SSE4/AVX/AVX2 instruction, it will cause a #NM. */
/* 	 * The system uses this feature to detect if a user uses these instructions, if it doesn't use, don't execute related context */
/* 	 * switch (save & restore the registers above). */
/* 	 *\/ */
/* 	cr0 = fpu_read_cr0(); */
/* 	val = status ? (cr0 & ~FPU_DISABLED_MASK) */
/* 	             : (cr0 | FPU_DISABLED_MASK); // ENABLE(status == 1) : DISABLE(status == 0) */
/* 	asm volatile("mov %0, %%cr0" : : "r"(val)); */

/* 	return; */
/* } */

/* static inline int */
/* fpu_disabled_exception_handler(void) */
/* { */
/* 	struct thread *curr_thd; */
/* 	curr_thd = *PERCPU_GET(active_thread); */
/* 	assert(curr_thd != NULL); */

/* 	/\* If this thread enables fpu and there is still a #NM, tell the handler to deal with *\/ */
/* 	if(!fpu_is_disabled()) return 0; */

/* 	curr_thd->fpregs.status = 1; */
/* 	fpu_switch(curr_thd); */

/* 	return 1; */
/* } */

static inline void
fpu_thread_init(struct fpu_regs *fpregs)
{
	memset(fpregs, 0, sizeof(struct fpu_regs));
	/* Have to set bit 63 of xcomp_bv to 1, or it will cause a #GP */
	fpregs->xcomp_bv |= ((u64_t)1 << 63);
	fpregs->cwd = 0x37f;
#if FPU_SUPPORT_SSE > 0
	/*
	 * Mask all SSE exceptions, this will make processor ingore the exceptions
	 * and the user program has to deal with invalid SSE results.
	 */
	fpregs->mxcsr = 0x1f80;
#endif
	return;
}

static inline void
fxsave(struct fpu_regs *regs)
{
#if FPU_SUPPORT_FXSR > 0
	asm volatile("fxsave %0" : "=m"(*regs));
#else
	asm volatile("fsave %0" : "=m"(*regs));
#endif
	return;
}

static inline void
fxrstor(struct fpu_regs *regs)
{
#if FPU_SUPPORT_FXSR > 0
	asm volatile("fxrstor %0" : : "m"(*regs));
#else
	asm volatile("frstor %0" : : "m"(*regs));
#endif
	return;
}

static inline void
xsaves(struct fpu_regs *regs)
{
#ifdef __x86_64__
	/* 0x7: XCR0_x87 | XCR0_x87 | XCR0_x87 */
	asm volatile("xsaves64 %0" : "=m"(*regs): "a"(0x7), "d"(0):"memory");
#else
	asm volatile("xsaves %0" : "=m"(*regs): "a"(0x7), "d"(0):"memory");
#endif
}

static inline void
xrestors(struct fpu_regs *regs)
{
#ifdef __x86_64__
	/* 0x7: XCR0_x87 | XCR0_x87 | XCR0_x87 */
	asm volatile("xrstors64 %0" : :"m"(*regs), "a"(0x7), "d"(0):"memory");
#else
	asm volatile("xrstors %0" : :"m"(*regs), "a"(0x7), "d"(0):"memory");
#endif
}

static inline void
fpu_save(struct fpu_regs *regs)
{
#if FPU_SUPPORT_XSAVES
	xsaves(regs);
#else
	fxsave(regs);
#endif
}

static inline void
fpu_restore(struct fpu_regs *regs)
{
#if FPU_SUPPORT_XSAVES
	xrestors(regs);
#else
	fxrstor(regs);
#endif
}

static inline void
fpu_switch(struct fpu_regs *prev, struct fpu_regs *next)
{
	fpu_save(prev);
	fpu_restore(next);
}

/* static inline void */
/* fpu_enable(void) */
/* { */
/* 	if (!fpu_is_disabled()) return; */

/* 	fpu_set(FPU_ENABLE); */
/* 	*PERCPU_GET(fpu_disabled) = 0; */

/* 	return; */
/* } */
