#ifndef CHAL_CONSTS_H
#define CHAL_CONSTS_H

struct pt_regs {
        unsigned long cpsr;
        unsigned long r0;
        unsigned long r1;
        unsigned long r2;
        unsigned long r3;
        unsigned long r4;
        unsigned long r5;
        unsigned long r6;
        unsigned long r7;
        unsigned long r8;
        unsigned long r9;
        unsigned long r10;
        unsigned long r11;
        unsigned long r12;
        unsigned long r13_sp;
        unsigned long r14_lr;
        unsigned long r15_pc;
};

#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PGD_SHIFT 20
#define PGD_RANGE (1 << PGD_SHIFT)
#define PGD_SIZE PGD_RANGE
#define PGD_MASK (~(PGD_RANGE - 1))
#define PGD_PER_PTBL 1024

#define CPSR_USER_LEVEL 0x600f0010

#define MAX_ASID_BITS 8
#define MAX_NUM_ASID (1<<MAX_ASID_BITS)
#define ASID_MASK (MAX_NUM_ASID - 1)

#endif /* CHAL_CONSTS_H */
