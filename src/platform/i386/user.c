#include "printk.h"
#include "gdt.h"
#include "user.h"
#include "types.h"

#define IA32_SYSENTER_CS 0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

void test_user_function(void);

static inline void
writemsr(uint32_t reg, uint32_t low, uint32_t high)
{
  __asm__("wrmsr" : : "c" (reg), "a"(low), "d"(high));
}

void
test_user_function(void)
{
  printk(INFO, "Hi, I'm a user function!\n");
}

void
user__init(void)
{
  writemsr(IA32_SYSENTER_CS, SEL_KCSEG, 0);
  writemsr(IA32_SYSENTER_ESP, 0, 0);
  writemsr(IA32_SYSENTER_EIP, (uint32_t)sysenter, 0);

  test_user_function();

  //  __asm__("sysexit");

     __asm__("mov $0x23, %ax");
     __asm__("mov %ax, %ds");
     __asm__("mov %ax, %es");
     __asm__("mov %ax, %fs");
     __asm__("mov %ax, %gs");
 
     __asm__("mov %esp, %eax");
     __asm__("push $0x23");
     __asm__("push %eax");
     __asm__("pushf");
     __asm__("push $0x1B");
     __asm__("push test_user_function");
     __asm__("iret");
}
