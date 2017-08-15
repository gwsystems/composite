/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COS_ASM_SERVER_STUB_SIMPLE_STACK_H
#define COS_ASM_SERVER_STUB_SIMPLE_STACK_H

#define __ASM__
#include "../../kernel/include/asm_ipc_defs.h"
#include <consts.h>
#include <cos_asm_simple_stacks.h>


/* 
 * The register layout is paired with that in ipc.S, %ecx holding the
 * spdid.  We zero out the %ebp so that is we do a stack trace later,
 * we know that when the %ebp is 0, we are at the end of the stack.
 */

#define cos_asm_server_stub(name) \
.globl name##_inv ;               \
.type  name##_inv, @function ;	  \
.align 16 ;			  \
name##_inv:                       \
        COS_ASM_GET_STACK         \
	pushl %ebp;		  \
	xor %ebp, %ebp;		  \
        pushl %edi;	          \
        pushl %esi;	          \
        pushl %ebx;	          \
        call name ; 		  \
        addl $16, %esp;           \
                                  \
        movl %eax, %ecx;          \
        movl $RET_CAP, %eax;	  \
        COS_ASM_RET_STACK         \
                                  \
        sysenter;

#define cos_asm_server_stub_spdid(name) \
.globl name##_inv ;                     \
.type  name##_inv, @function ;	        \
.align 16 ;			        \
name##_inv:                             \
        COS_ASM_GET_STACK               \
	pushl %ebp;		        \
	xor %ebp, %ebp;			\
        pushl %edi;	                \
        pushl %esi;	                \
        pushl %ecx;	                \
        call name ; 		        \
        addl $16, %esp;                 \
                                        \
        movl %eax, %ecx;                \
        movl $RET_CAP, %eax;	        \
        COS_ASM_RET_STACK		\
                                        \
        sysenter;

#endif /* COS_ASM_SERVER_STUB_H */
