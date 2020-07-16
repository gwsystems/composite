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

#endif /* CHAL_CONSTS_H */
