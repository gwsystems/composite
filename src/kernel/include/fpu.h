#include "thread.h"

#ifndef FPU_H
#define FPU_H

#define FPU_DISABLED 0x00000008

//extern struct thread *last_used_fpu;

struct cos_fpu
{
        u16_t cwd; /* FPU Control Word*/
        u16_t swd; /* FPU Status Word */
        u16_t twd; /* FPU Tag Word */
        u16_t fip; /* FPU IP Offset */
        u16_t fcs; /* FPUIP Selector */
        unsigned int foo; /* FPU Operand Pointer Offset */
        unsigned int fos; /* FPU Operand Pointer Selector */

        /* 8*10 bytes for each FP-reg = 80 bytes: */
        unsigned int st_space[20]; /* 8 data registers */
	int status; /* used fpu */
	int saved_fpu;
};

struct cos_fpu_struct {
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

} __attribute__((aligned(16)));

inline void fxsave(struct thread*);
inline void fxrstor(struct thread*);
inline void fsave(struct thread*);
inline void frstor(struct thread*);

inline void fpu_disable(void);
inline void fpu_enable(void);
int fpu_save(struct thread *curr, struct thread *next);
int fpu_is_disabled(void);
int fpu_thread_uses_fp(struct thread *thd);
unsigned int fpu_read_cr0(void);

struct thread* fpu_get_last_used(void);
#endif
