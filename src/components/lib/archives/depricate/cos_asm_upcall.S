/**
 * Copyright 2007 by Boston University, Gabriel Parmer,
 * gabep1@cs.bu.edu, and The George Washington University,
 * Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define __ASM__

#include "../../../kernel/include/asm_ipc_defs.h"
#include <cos_asm_stacks.h>
#include <consts.h>

#define IPRETURN 4

.section .initonce
.align 32
nil:
	.rep 1024
	.long 0
	.endr

.globl cos_static_stack
cos_static_stack:
	.rep ALL_STACK_SZ
	.long 0
	.endr
.globl cos_static_stack_end
cos_static_stack_end:

.section .initonce
.globl stkmgr_stack_space
stkmgr_stack_space:
	.rep ALL_TMP_STACKS_SZ
	.long 0
	.endr

.text
.globl __cosrt_upcall_entry
.type  __cosrt_upcall_entry, @function
.align 16
__cosrt_upcall_entry:
	COS_ASM_GET_STACK

	xor %ebp, %ebp
	pushl %esi
	pushl %edi
	pushl %ebx
	pushl %ecx /* option */
	call cos_upcall_fn
	addl $16, %esp

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */
	COS_ASM_RET_STACK

	sysenter
	COS_ASM_REQUEST_STACK
