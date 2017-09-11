#ifndef FPU_REGS_H
#define FPU_REGS_H


struct cos_fpu {
#ifdef FPU_ENABLED
	u16_t cwd; /* Control Word */
	u16_t swd; /* Status Word */
	u16_t twd; /* Tag Word */
	u16_t fop; /* Last Instruction Opcode */
	union {
		struct {
			u64_t rip; /* Instruction Pointer */
			u64_t rdp; /* Data Pointer */
		};
		struct {
			u32_t fip; /* FPU IP Offset */
			u32_t fcs; /* FPU IP Selector */
			u32_t foo; /* FPU Operand Offset */
			u32_t fos; /* FPU Operand Selector */
		};
	};
	u32_t mxcsr;      /* MXCSR Register State */
	u32_t mxcsr_mask; /* MXCSR Mask */

	/* 8*16 bytes for each FP-reg = 128 bytes: */
	u32_t st_space[32];

	/* 16*16 bytes for each XMM-reg = 256 bytes: */
	u32_t xmm_space[64];

	u32_t padding[12];

	union {
		u32_t padding1[12];
		u32_t sw_reserved[12];
	};
	int status;
#endif
} __attribute__((aligned(16)));

#endif
