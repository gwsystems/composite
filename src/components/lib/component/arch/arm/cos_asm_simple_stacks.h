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
#define COS_ASM_ALLOC_SP	\
	sub sp, sp, #0x04 ;

#define COS_ASM_CLEANUP_TLS	\
	COS_ASM_ALLOC_SP	\
	mov r6, #0x00 ;		\
	str r6, [sp] ;

#define COS_ASM_SAFE_SP		\
	sub sp, sp, #0x0c ;

/* clang-format off */
#define COS_ASM_GET_STACK_BASIC			\
	ldr r6, =cos_static_stack ;		\
	add r6, r6, #COS_STACK_SZ ;		\
	lsr r6, r6, #MAX_STACK_SZ_BYTE_ORDER ;	\
	lsl r6, r6, #MAX_STACK_SZ_BYTE_ORDER ;	\
	mov r12, r6 ;				\
	mov r6, r0 ;				\
	lsl r6, r6, #0x10 ;			\
	lsr r6, r6, #0x10 ;			\
	lsl r6, r6, #MAX_STACK_SZ_BYTE_ORDER ;	\
	add sp, r12, r6 ;			\
	lsr r6, r6, #MAX_STACK_SZ_BYTE_ORDER ;	\
	lsr r0, r0, #0x10 ;			\
	COS_ASM_ALLOC_SP			\
	str r0, [sp] ;				\
	COS_ASM_ALLOC_SP			\
	str r6, [sp] ;				\
	COS_ASM_CLEANUP_TLS			\
	COS_ASM_SAFE_SP

#define COS_ASM_GET_STACK		\
	COS_ASM_GET_STACK_BASIC		\
	COS_ASM_ALLOC_SP		\
	mov r6, #0x00;			\
	str r6, [sp];

#define COS_ASM_GET_STACK_INVTOKEN	\
	COS_ASM_GET_STACK_BASIC		\
	COS_ASM_ALLOC_SP		\
	str r1, [sp];

#define COS_ASM_RET_STACK

#define COS_ASM_REQUEST_STACK

/* clang-format on */

#endif
