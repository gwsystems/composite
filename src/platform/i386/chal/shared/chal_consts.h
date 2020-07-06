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


#endif /* CHAL_CONSTS_H */
