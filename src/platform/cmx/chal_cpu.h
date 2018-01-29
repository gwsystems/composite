#ifndef CHAL_CPU_H
#define CHAL_CPU_H

#include <pgtbl.h>
#include <thd.h>
#include "stm32f7xx_hal.h"

extern u32_t Stack_Mem[ALL_STACK_SZ];
extern void
cos_thd_set_tid_cpuid(int tid, int cpuid);

/* FIXME: I doubt these flags are really the same as the PGTBL_* macros */
static inline u32_t
chal_cpu_fault_errcode(struct pt_regs *r) { return r->r14_lr; }

static inline u32_t
chal_cpu_fault_ip(struct pt_regs *r) { return r->r15_pc; }

extern unsigned int _c1_sdata;
extern unsigned int _c1_edata;
extern unsigned int __c1_bss_start__;
extern unsigned int __c1_bss_end__;

/* PRY:we used naked here to pass parameters directly according to arm calling convention.
 * No need to make this inline because we only use this once. */
static void __attribute__((naked))
chal_user_upcall(void *ip, u16_t tid)
{
	/* Now we switch the execution to user space, and begin to use the PSP stack pointer */
	__asm__ __volatile__("ldr r2,=Stack_Mem \n\t" \
			     "add r2,#0x200 \n\t" \
			     /* This is to avoid overwriting the PC pointer on return */
			     "sub r2,#0x10 \n\t" \
			     "msr psp,r2 \n\t" \
			     "mov r2,#0x03 \n\t" \
			     "msr control,r2 \n\t" \
			     "mov r4,r0 \n\t" \

			     "mov r0,r1 \n\t" \
			     "mov r1,0x00 \n\t" \
			     "bl cos_thd_set_tid_cpuid \n\t" \

			     "mov r0,#0x00 \n\t" \
			     "mov r1,#0x00 \n\t" \
			     "bx  r4 \n\t" \
			     "b   .\n\t" \
	    		     ::: "memory", "cc");
}

static inline void
chal_cpuid(int code, u32_t *a, u32_t *b, u32_t *c, u32_t *d)
{  }

#endif /* CHAL_CPU_H */
