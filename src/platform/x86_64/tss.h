#ifndef TSS_H
#define TSS_H

#include "shared/cos_types.h"
struct tss {
    u32_t resv0;
    u64_t rsp0;
    u64_t rsp1;
    u64_t rsp2;
    u64_t resv1;
    u64_t ist1;
    u64_t ist2;
    u64_t ist3;
    u64_t ist4;
    u64_t ist5;
    u64_t ist6;
    u64_t ist7;
    u64_t resv2;
    u16_t resv3;
    u16_t bitmap;
} __attribute__((packed));

struct kernel_stack_info {
	vaddr_t kernel_stack_addr;
	vaddr_t user_stack_addr;
};

extern struct tss tss[NUM_CPU];
extern struct kernel_stack_info kernel_stack_info[NUM_CPU];

#endif /* TSS_H */
