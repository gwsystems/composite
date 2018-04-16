#ifndef TSS_H
#define TSS_H

#include "shared/cos_types.h"

struct tss {
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
	u32_t es;
	u32_t cs;
	u32_t ss;
	u32_t ds;
	u32_t fs;
	u32_t gs;
	u32_t ldt;
	u16_t trace;
	u16_t bitmap;
};

extern struct tss tss[NUM_CPU];

#endif /* TSS_H */
