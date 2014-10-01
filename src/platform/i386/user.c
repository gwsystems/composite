#include "shared/consts.h"
#include "shared/cos_types.h"
#include "printk.h"
#include "gdt.h"
#include "user.h"
#include "tss.h"
#include "vm.h"

#define IA32_SYSENTER_CS 0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

extern u32_t user_entry_point;
extern u32_t user_stack_address;

static inline void
writemsr(u32_t reg, u32_t low, u32_t high)
{
	__asm__("wrmsr" : : "c"(reg), "a"(low), "d"(high));
}

void
user__init(void)
{
	writemsr(IA32_SYSENTER_CS, SEL_KCSEG, 0);
	writemsr(IA32_SYSENTER_ESP, (u32_t)tss_get()->esp0, 0);
	writemsr(IA32_SYSENTER_EIP, (u32_t)&sysenter, 0);

	printk (INFO, "SYSENTER Seg 0x%x, ESP 0x%x, EIP 0x%x\n", SEL_KCSEG, tss_get()->esp0, &sysenter);
	printk (INFO, "About to jump to 0x%x with stack at 0x%x\n", user_entry_point, user_stack_address);

	__asm__("sysexit" : : "c"(user_stack_address), "d"(user_entry_point));
}
