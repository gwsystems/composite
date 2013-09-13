#include "user.h"

#define IA32_SYSENTER_CS  "0x174"
#define IA32_SYSENTER_ESP "0x175"
#define IA32_SYSENTER_EIP "0x176"

extern void sysenter(void);

void
user__init (void)
{
  void *sef = &sysenter;
  asm volatile("mov %edx,0");
  asm volatile("mov %%eax,%0" : "=r"(sef));
  asm volatile("mov %ecx," IA32_SYSENTER_EIP);
  asm volatile("wrmsr");
  //asm volatile("sysexit");
}
