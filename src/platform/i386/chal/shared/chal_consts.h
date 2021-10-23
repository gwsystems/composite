#ifndef CHAL_CONSTS_H
#define CHAL_CONSTS_H

struct pt_regs {
        long bx;
        long cx;
        long dx;
        long si;
        long di;
        long bp;
        long ax;
        long ds;
        long es;
        long fs;
        long gs;
        long orig_ax;
        long ip;
        long cs;
        long flags;
        long sp;
        long ss;
};

#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PGD_SHIFT 22
#define PGD_RANGE (1 << PGD_SHIFT)
#define PGD_SIZE PGD_RANGE
#define PGD_MASK (~(PGD_RANGE - 1))
#define PGD_PER_PTBL 1024

#define PGT1_SHIFT 30
#define PGT1_RANGE (1UL << PGT1_SHIFT)
#define PGT1_SIZE PGT1_RANGE
#define PGT1_MASK (~(PGT1_RANGE - 1))
#define PGT1_PER_PTBL 512

#define PGT2_SHIFT 21
#define PGT2_RANGE (1UL << PGT2_SHIFT)
#define PGT2_SIZE PGT2_RANGE
#define PGT2_MASK (~(PGT2_RANGE - 1))
#define PGT2_PER_PTBL 512

#define PGT3_SHIFT 12
#define PGT3_RANGE (1UL << PGT3_SHIFT)
#define PGT3_SIZE PGT3_RANGE
#define PGT3_MASK (~(PGT3_RANGE - 1))
#define PGT3_PER_PTBL 512

#endif /* CHAL_CONSTS_H */
