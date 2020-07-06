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

#endif /* CHAL_CONSTS_H */
