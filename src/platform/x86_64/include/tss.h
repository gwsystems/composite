#ifndef TSS_H
#define TSS_H

#include <types.h>
#include <consts.h>

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

extern struct tss tss[COS_NUM_CPU];

#endif /* TSS_H */
