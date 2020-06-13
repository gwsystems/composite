#ifndef CHAL_CPU_H
#define CHAL_CPU_H

#include <pgtbl.h>
#include <thd.h>


static void
chal_cpu_pgtbl_activate(pgtbl_t pgtbl)
{
	pgtbl_update(pgtbl);
}


extern void sysenter_entry(void);

static void
chal_cpu_init(void)
{
}

static inline vaddr_t
chal_cpu_fault_vaddr(struct pt_regs *r)
{
}

/* FIXME: I doubt these flags are really the same as the PGTBL_* macros */
static inline u32_t
chal_cpu_fault_errcode(struct pt_regs *r)
{
	return r->r0;
}

static inline u32_t
chal_cpu_fault_ip(struct pt_regs *r)
{
	return r->r15_pc;
}

static inline void
chal_user_upcall(void *ip, u16_t tid, u16_t cpuid)
{
	/* Now we need to go into the user-level. Need to load something to
	 * there for execution. We can use mbooter to have a try.
	 * Now the timers are not hooked. After all it can be considered to be
	 * incorrect because the RDTSC issue and timer reprogramming.
	 * We must use the global timer instead. */
	__cos_enter_user_mode(ip, (((unsigned long)cpuid) << 16) | tid);
}

void chal_timer_thd_init(struct thread *t);

#endif /* CHAL_CPU_H */
