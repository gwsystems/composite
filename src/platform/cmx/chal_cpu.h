#ifndef CHAL_CPU_H
#define CHAL_CPU_H

#include <pgtbl.h>
#include <thd.h>


u32_t comp1_stack[1000];

/* FIXME: I doubt these flags are really the same as the PGTBL_* macros */
static inline u32_t
chal_cpu_fault_errcode(struct pt_regs *r) { return r->orig_r4; }

static inline u32_t
chal_cpu_fault_ip(struct pt_regs *r) { return r->r15_pc; }

static inline void
chal_user_upcall(void *ip, u16_t tid)
{
	/* Now we switch the execution to user space, and begin to use the PSP */
	__asm__ __volatile__( \
					    "ldr r0,=comp1_stack \n\t" \
						"msr psp,r0 \n\t" \
						"mov r0,#0x02 \n\t" \
						"msr control,r0 \n\t"
	    				:
					    : \
	    				: "memory", "cc", "r0");

    void(*ptr)(void)=(void(*)(void))(ip);
    /* Just call the component, and never returns */
    ptr();
}

static inline void
chal_cpuid(int code, u32_t *a, u32_t *b, u32_t *c, u32_t *d)
{  }

#endif /* CHAL_CPU_H */
