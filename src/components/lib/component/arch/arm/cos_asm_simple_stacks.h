#ifndef COS_ASM_SIMPLE_STACKS_ARM_H
#define COS_ASM_SIMPLE_STACKS_ARM_H

/* 
 * r0 contains thdid+cpuid in sinv or upcall
 * r1 contains token for sinv.
 * r2 - r5 are also used in calling conventions.
 * avoid clobbering those registers here.
 *
 * see platform/armv7/chal/call_convention.h for more details.
 */
/*
 * NOTE: stack will be aligned to 8byte boundary
 * we pushed cpuid, thdid, invtoken and a placeholder for invcap, that's 16B
 * so, do nothing more to align!
 */
#define COS_ASM_ALIGN_8B_SP

/* Placeholder for INVCAP */
#define COS_ASM_INVCAP_SLOT			\
	mov r6, #0;				\
	push {r6};

/* clang-format off */
#define COS_ASM_GET_STACK_BASIC			\
	ldr r6, =cos_static_stack;		\
	add r6, r6, #COS_STACK_SZ;		\
	mov r12, r6;				\
	mov r6, r0;				\
	and r6, r6, #0xff;			\
	lsl r6, r6, #MAX_STACK_SZ_BYTE_ORDER;	\
	add sp, r12, r6;			\
	lsr r6, r6, #MAX_STACK_SZ_BYTE_ORDER;	\
	lsr r0, r0, #16;			\
	push {r0};				\
	push {r6};

#define COS_ASM_GET_STACK			\
	COS_ASM_GET_STACK_BASIC			\
	mov r6, #0;				\
	push {r6};				\
	COS_ASM_INVCAP_SLOT			\
	COS_ASM_ALIGN_8B_SP

#define COS_ASM_GET_STACK_INVTOKEN		\
	COS_ASM_GET_STACK_BASIC			\
	push {r1};				\
	COS_ASM_INVCAP_SLOT			\
	COS_ASM_ALIGN_8B_SP

#define COS_ASM_RET_STACK

#define COS_ASM_REQUEST_STACK

/* clang-format on */

#endif
