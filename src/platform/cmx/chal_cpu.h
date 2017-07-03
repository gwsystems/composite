#ifndef CHAL_CPU_H
#define CHAL_CPU_H

#include <pgtbl.h>
#include <thd.h>
#include "stm32f7xx_hal.h"

extern u32_t comp1_stack[1024];

/* FIXME: I doubt these flags are really the same as the PGTBL_* macros */
static inline u32_t
chal_cpu_fault_errcode(struct pt_regs *r) { return r->r14_lr; }

static inline u32_t
chal_cpu_fault_ip(struct pt_regs *r) { return r->r15_pc; }

extern unsigned int _c1_sdata;
extern unsigned int _c1_edata;
extern unsigned int __c1_bss_start__;
extern unsigned int __c1_bss_end__;

static inline void
chal_user_upcall(void *ip, u16_t tid)
{
	/* Now we switch the execution to user space, and begin to use the PSP stack pointer */
	__asm__ __volatile__("ldr r2,=comp1_stack \n\t" \
			     "add r2,#0x1000 \n\t" \
			     "msr psp,r2 \n\t" \
			     "mov r2,#0x03 \n\t" \
			     "msr control,r2 \n\t" \
			     "mov r3,r0 \n\t" \
			     "mov r0,#0x00 \n\t" \
			     "mov r1,#0x00 \n\t" \
			     "bx  r3 \n\t" \
			     "b   .\n\t" \
	    		     ::: "memory", "cc");
}

static inline void
chal_cpuid(int code, u32_t *a, u32_t *b, u32_t *c, u32_t *d)
{  }

#endif /* CHAL_CPU_H */
