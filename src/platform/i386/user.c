#include "user.h"
#include "types.h"

extern uintptr_t gdt_base;

#define IA32_SYSENTER_CS 0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

static inline void
writemsr(uint32_t reg, uint32_t low, uint32_t high)
{
  __asm__("wrmsr" : : "c" (reg), "a"(low), "d"(high));
}

void
user__init(void)
{
  writemsr(IA32_SYSENTER_CS, gdt_base+8, 0);
  writemsr(IA32_SYSENTER_ESP, 0, 0);
  writemsr(IA32_SYSENTER_EIP, (uint32_t)sysenter, 0);
  __asm__("sysexit");
}
