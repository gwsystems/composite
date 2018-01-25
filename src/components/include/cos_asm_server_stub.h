/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * 2011 by Gabriel Parmer, gparmer@gwu.edu.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COS_ASM_SERVER_STUB_H
#define COS_ASM_SERVER_STUB_H

#include "../../kernel/include/asm_ipc_defs.h"
#include <cos_asm_stacks.h>

/* 
 * The register layout is paired with that in ipc.S, %ecx holding the
 * spdid.  We zero out the %ebp so that is we do a stack trace later,
 * we know that when the %ebp is 0, we are at the end of the stack.
 */

/* clang-format off */
#define cos_asm_server_fn_stub(name, fn)	\
.globl name##_inv ;               \
.type  name##_inv, @function ;	  \
.align 16 ;			  \
name##_inv:                       \
        COS_ASM_GET_STACK         \
	pushl $0;		  \
	pushl $0;		  \
	pushl %esp;		  \
	pushl %ebp;		  \
	xor %ebp, %ebp;		  \
        pushl %edi;	          \
        pushl %esi;	          \
        pushl %ebx;	          \
        call fn ;		  \
        addl $20, %esp;           \
        popl %esi;		  \
        popl %edi;		  \
        movl %eax, %ecx;          \
        movl $RET_CAP, %eax;	  \
        COS_ASM_RET_STACK         \
                                  \
        sysenter;                 \
        COS_ASM_REQUEST_STACK

#define cos_asm_server_stub(name) cos_asm_server_fn_stub(name, name)

#define cos_asm_server_fn_stub_token(name, fn)	\
.globl name##_inv ;                     \
.type  name##_inv, @function ;	        \
.align 16 ;			        \
name##_inv:                             \
        COS_ASM_GET_STACK               \
	pushl $0;			\
	pushl $0;			\
	pushl %esp;			\
	pushl %ebp;		        \
	xor %ebp, %ebp;			\
        pushl %edi;	                \
        pushl %esi;	                \
        pushl %ecx;	                \
        call fn ; 		        \
        addl $20, %esp;                 \
        popl %esi;			\
        popl %edi;			\
        movl %eax, %ecx;                \
        movl $RET_CAP, %eax;	        \
        COS_ASM_RET_STACK		\
                                        \
        sysenter;                       \
        COS_ASM_REQUEST_STACK
/* clang-format on */

#define cos_asm_server_stub_token(name) cos_asm_server_fn_stub_token(name, name)

#endif /* COS_ASM_SERVER_STUB_H */
