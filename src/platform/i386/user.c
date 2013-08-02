#include "user.h"

extern void sysenter(void);

void
user__init (void)
{
  void *sef = &sysenter;
  asm volatile("mov %edx,0");
  asm volatile("mov %%eax,%0" : "=r"(sef));
  asm volatile("mov %ecx,0x176");
  asm volatile("wrmsr");
}
