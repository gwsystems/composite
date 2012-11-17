#include "thread.h"

#ifndef FPU_H
#define FPU_H

struct cos_fpu {
        unsigned int cwd; /* FPU Control Word*/
        unsigned int swd; /* FPU Status Word */
        unsigned int twd; /* FPU Tag Word */
        unsigned int fip; /* FPU IP Offset */
        unsigned int fcs; /* FPUIP Selector */
        unsigned int foo; /* FPU Operand Pointer Offset */
        unsigned int fos; /* FPU Operand Pointer Selector */

        /* 8*10 bytes for each FP-reg = 80 bytes: */
        unsigned int st_space[20]; /* 8 data registers */
	unsigned int status; /* 1.USED FPU 2.NOT USED FPU */
};

inline void fsave(struct cos_fpu*);
inline void frstor(struct cos_fpu*);
inline void disable_fpu(void);
inline void enable_fpu(void);

unsigned int cos_read_cr0(void);
void print_cr0(void);

#endif
