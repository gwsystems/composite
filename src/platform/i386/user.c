#include "printk.h"
#include "gdt.h"
#include "user.h"
#include "types.h"
#include "tss.h"

#define IA32_SYSENTER_CS 0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

extern uint32_t *base_user_address;
extern uint32_t test_function_offset;
uint32_t *test_function_address;
void test_user_function(void);

static inline void
writemsr(uint32_t reg, uint32_t low, uint32_t high)
{
  __asm__("wrmsr" : : "c" (reg), "a"(low), "d"(high));
}

void
test_user_function(void)
{
  __asm__("sysenter");
}


uint32_t user_mode_stack;

void
user__init(void)
{
  user_mode_stack = (uint32_t)base_user_address + (32 * 1024);
  test_function_address = (uint32_t*) ((uint32_t)base_user_address | test_function_offset);

  writemsr(IA32_SYSENTER_CS, SEL_KCSEG, 0);
  writemsr(IA32_SYSENTER_ESP, (uint32_t)tss_get()->esp0, 0);
  writemsr(IA32_SYSENTER_EIP, (uint32_t)&sysenter, 0);

  printk (INFO, "SYSENTER Seg 0x%x, ESP 0x%x, EIP 0x%x\n", SEL_KCSEG, tss_get()->esp0, &sysenter);
  printk (INFO, "Using user mode base address 0x%x and offset 0x%x\n", base_user_address, test_function_offset);
  printk (INFO, "About to jump to 0x%x with stack at 0x%x\n", test_function_address, user_mode_stack);

  __asm__("sysexit" : : "a" (user_mode_stack), "d"(test_function_address));
}
