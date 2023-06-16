#pragma once

/*
 * Kernel implementation constants
 */

#define COS_INVSTK_SIZE      2

#define FPU_ENABLED          1
#define FPU_SUPPORT_SSE      1
#define FPU_SUPPORT_FXSR     1 /* >0 : CPU supports FXSR. */
#define FPU_SUPPORT_XSAVE    1
#define FPU_SUPPORT_XSAVEOPT 1
#define FPU_SUPPORT_XSAVEC   1
#define FPU_SUPPORT_XSAVES   1
#define ENABLE_SERIAL        1
