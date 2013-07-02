#include "thread.h"
#include "per_cpu.h"

#ifndef FPU_H
#define FPU_H

#define ENABLE  1
#define DISABLE 0
#define FPU_DISABLED_MASK 0x8

struct cos_fpu {
        u16_t 			  cwd; /* Control Word */
        u16_t 			  swd; /* Status Word */
        u16_t                     twd; /* Tag Word */
        u16_t                     fop; /* Last Instruction Opcode */
        union {
                struct {
                        u64_t     rip; /* Instruction Pointer */
                        u64_t     rdp; /* Data Pointer */
                };
                struct {
                        u32_t     fip; /* FPU IP Offset */
                        u32_t     fcs; /* FPU IP Selector */
                        u32_t     foo; /* FPU Operand Offset */
                        u32_t     fos; /* FPU Operand Selector */
                };
        };
        u32_t                     mxcsr;          /* MXCSR Register State */
        u32_t                     mxcsr_mask;     /* MXCSR Mask */

        /* 8*16 bytes for each FP-reg = 128 bytes: */
        u32_t                     st_space[32];

        /* 16*16 bytes for each XMM-reg = 256 bytes: */
        u32_t                     xmm_space[64];

        u32_t                     padding[12];

        union {
                u32_t             padding1[12];
                u32_t             sw_reserved[12];
        };
        int status;
        int saved_fpu;

} __attribute__((aligned(16)));

static int fpu_disabled;
static struct thread *last_used_fpu;

/**
 * these functions should not be called outside fpu.c, make them static?
 */
inline void fxsave(struct thread*);
inline void fxrstor(struct thread*);
inline int fpu_thread_uses_fp(struct thread *thd);
inline unsigned long fpu_read_cr0(void);
inline void fpu_set(int);
inline struct thread* fpu_get_last_used(void);

int fpu_init(void);
int fpu_save(struct thread *curr, struct thread *next);
void fpu_enable(void);
void fpu_disable(void);
int fpu_is_disabled(void);
#endif
