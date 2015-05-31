/* Based on code from Pintos. See LICENSE.pintos for licensing information */
#ifndef TSS_H
#define TSS_H

#include "shared/cos_types.h"

struct tss
{
    u16_t back_link, :16;
    void *esp0;                         /* Ring 0 stack virtual address. */
    u16_t ss0, :16;                  /* Ring 0 stack segment selector. */
    void *esp1;
    u16_t ss1, :16;
    void *esp2;
    u16_t ss2, :16;
    u32_t cr3;
    void (*eip) (void);
    u32_t eflags;
    u32_t eax, ecx, edx, ebx;
    u32_t esp, ebp, esi, edi;
    u16_t es, :16;
    u16_t cs, :16;
    u16_t ss, :16;
    u16_t ds, :16;
    u16_t fs, :16;
    u16_t gs, :16;
    u16_t ldt, :16;
    u16_t trace, bitmap;
};

extern struct tss tss;

#endif /* TSS_H */
