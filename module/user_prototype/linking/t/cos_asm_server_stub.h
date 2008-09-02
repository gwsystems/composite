/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COS_ASM_SERVER_STUB_H
#define COS_ASM_SERVER_STUB_H

#define RET_CAP ((1<<20)-1)
#include <cos_asm_stacks.h>

#define cos_asm_server_stub(name) \
.globl name##_inv ;               \
.type  name##_inv, @function ;	  \
.align 16 ;			  \
name##_inv:                       \
        COS_ASM_GET_STACK         \
	pushl %ebp;		  \
        pushl %edi;	          \
        pushl %esi;	          \
        pushl %ebx;	          \
        call name ; 		  \
                                  \
        movl %eax, %ecx;          \
        movl $RET_CAP, %eax;	  \
                                  \
        sysenter;

#define cos_asm_server_stub_spdid(name) \
.globl name##_inv ;                     \
.type  name##_inv, @function ;	        \
.align 16 ;			        \
name##_inv:                             \
        COS_ASM_GET_STACK               \
	pushl %ebp;		        \
        pushl %edi;	                \
        pushl %esi;	                \
        pushl %ecx;	                \
        call name ; 		        \
                                        \
        movl %eax, %ecx;                \
        movl $RET_CAP, %eax;	        \
                                        \
        sysenter;

#endif /* COS_ASM_SERVER_STUB_H */
