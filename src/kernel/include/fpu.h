#include "thread.h"

#ifndef FPU_H
#define FPU_H

struct cos_fpu {
        long cwd; /* FPU Control Word*/
        long swd; /* FPU Status Word */
        long twd; /* FPU Tag Word */
        long fip; /* FPU IP Offset */
        long fcs; /* FPUIP Selector */
        long foo; /* FPU Operand Pointer Offset */
        long fos; /* FPU Operand Pointer Selector */

        /* 8*10 bytes for each FP-reg = 80 bytes: */
        long st_spaces[20]; /* 8 data registers */
	//long empty_spaces[512];
};

void fsave(struct thread*);
void frstor(struct thread*);

#endif
