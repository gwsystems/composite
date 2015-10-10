#ifndef TSS_H
#define TSS_H

#include "shared/cos_types.h"

struct tss
{
    u32_t back_link;
    u32_t esp0;
    u32_t ss0;
    u32_t esp1;
    u32_t ss1;
    u32_t esp2;
    u32_t ss2;
    u32_t cr3;
    u32_t eip;
    u32_t eflags;
    u32_t eax;
    u32_t ecx;
    u32_t edx;
    u32_t ebx;
    u32_t esp;
    u32_t ebp;
    u32_t esi;
    u32_t edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldt;
    u16_t trace;
    u16_t bitmap;
};

extern struct tss tss;

#endif /* TSS_H */
