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
	/* Above is lagecy 512 bytes area */

	union {
		struct {
			/* XSAVE Header, followed by reserved area */
			u64_t xstate_bv;
			u64_t xcomp_bv;
		};
		u8_t header_area[64];
	};

	/* Offset here should be at 576 bytes */

	/*
	 * 800 is calculated by sizeof(struct thread) - 576,
	 * with a little reserved area left in struct thread. This
	 * is to make sure all members of struct thread is still
	 * in a single page. This should be OK because the size
	 * is big enough to save all SSE and AVX2 state components.
	 * 
	 * Note this will not work with AVX512! You have to make sure
	 * closing to save AVX512 component in XCR0.
	 */
	u8_t xsave_ext_area[800]; 

	int status;
#endif
} __attribute__((aligned(64)));

#endif
