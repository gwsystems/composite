/**
 * Copyright 2020 by Phani Kishore Gadepalli, phanikishoreg@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "cos_asm_simple_stacks.h"

#define COS_STATIC_STACK                \
.align 16;                              \
.globl cos_static_stack;		\
cos_static_stack:			\
	.rept ALL_STACK_SZ_FLAT;	\
	.long 0;			\
	.endr;				\
.globl cos_static_stack_end;            \
cos_static_stack_end:

#define COS_UPCALL_ENTRY                \
.text;                                  \
.globl __cosrt_upcall_entry;            \
.type __cosrt_upcall_entry, %function;  \
.align 16;                              \
__cosrt_upcall_entry:                   \
	COS_ASM_GET_STACK		\
	mov r0, #0;			\
	mov r1, r2;			\
	mov r2, r3;			\
	mov r3, r4;			\
	ldr r12, =cos_upcall_fn;	\
	blx r12;			\
	mov r0, #0;			\
	.ltorg;

#define COS_ATOMIC_CMPXCHG	\
	nop;

#define COS_ATOMIC_CMPXCHG_END	\
	nop;

#define COS_ATOMIC_USER4_END	\
	nop;
