#include "user.h"
#include "types.h"

extern uintptr_t gdt_base;

#define IA32_SYSENTER_CS 0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

struct tss
  {
    uint16_t back_link, :16;
    void *esp0;                         /* Ring 0 stack virtual address. */
    uint16_t ss0, :16;                  /* Ring 0 stack segment selector. */
    void *esp1;
    uint16_t ss1, :16;
    void *esp2;
    uint16_t ss2, :16;
    uint32_t cr3;
    void (*eip) (void);
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint16_t es, :16;
    uint16_t cs, :16;
    uint16_t ss, :16;
    uint16_t ds, :16;
    uint16_t fs, :16;
    uint16_t gs, :16;
    uint16_t ldt, :16;
    uint16_t trace, bitmap;
  };
static struct tss tss;

static inline void
writemsr(uint32_t reg, uint32_t low, uint32_t high)
{
  __asm__("wrmsr" : : "c" (reg), "a"(low), "d"(high));
}

void
user__init(void)
{
  uint32_t esp;

  tss.ss0 = 0x92; // kernel data segment
  tss.bitmap = 0xdfff;

  __asm__("mov %%esp, %0" : "=g" (esp));
  tss.esp0 = (uint32_t*)((esp | (uint32_t)(4095)) + 4096);
  
  writemsr(IA32_SYSENTER_CS, gdt_base+8, 0);
  writemsr(IA32_SYSENTER_ESP, (uint32_t)tss.esp0, 0);
  writemsr(IA32_SYSENTER_EIP, (uint32_t)sysenter, 0);
  __asm__("sysexit");
}
