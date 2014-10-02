#include "kernel.h"
#include "tss.h"

#define IA32_SYSENTER_CS 0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

extern u32_t user_entry_point;
extern u32_t user_stack_address;
extern void sysenter_interposition_entry(void);

static inline void
writemsr(u32_t reg, u32_t low, u32_t high)
{
	__asm__("wrmsr" : : "c"(reg), "a"(low), "d"(high));
}

void
user_init(void)
{
	writemsr(IA32_SYSENTER_CS, SEL_KCSEG, 0);
	writemsr(IA32_SYSENTER_ESP, (u32_t)tss.esp0, 0);
	writemsr(IA32_SYSENTER_EIP, (u32_t)sysenter_interposition_entry, 0);

	printk (INFO, "About to jump to 0x%x with stack at 0x%x\n", user_entry_point, user_stack_address);

	__asm__("sti");
	__asm__("sysexit" : : "c"(user_stack_address), "d"(user_entry_point));
}
