/**
 * Copyright 2007 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Authors: Gabriel Parmer, gabep1@cs.bu.edu, 2007
 *          Qi Wang, interwq@gwu.edu,         2014
 */

#ifndef IPC_H
#define IPC_H

#include "thread.h"
#include "spd.h"
#include "asm_ipc_defs.h"

void ipc_init(void);

/* Composite sysenter entry */

#define STR_CONC(x) #x
#define STR(x) STR_CONC(x)

#define SAVE_REGS_ASM		\
	"subl $40, %esp\n\t" 	\
	"pushl %eax\n\t"	\
	"pushl %ebp\n\t"	\
	"pushl %edi\n\t"	\
	"pushl %esi\n\t"	\
	"pushl %edx\n\t"	\
	"pushl %ecx\n\t"	\
	"pushl %ebx\n\t"

#define RESTORE_REGS_ASM	\
	"popl %ebx\n\t"		\
	"popl %ecx\n\t"		\
	"popl %edx\n\t"		\
	"popl %esi\n\t"		\
	"popl %edi\n\t"		\
	"popl %ebp\n\t"		\
	"popl %eax\n\t"		\
	"addl $40, %esp\n\t"

#define RET_TO_USER    \
	"pushl $0\n\t" \
	"pushl $0\n\t" \
	"sti\n\t"      \
	"sysexit"

/* Since gcc doesn't suppport __attribute__((naked)) on x86, we have
 * to write the entire function in top-level asm to avoid generating
 * prologue (which corrupts ebp). */

/*
 * Composite invocations are indicated by the contents of %eax:
 * +-------------+----+--...
 * |   cap_inv   | sc | normal OS (linux) syscalls
 * +-------------+----+--...
 * 32            |    COS_SYSCALL_OFFSET
 *               COS_CAPABILITY_OFFSET
 *	
 * Where each character represents a bit.
 * cap_inv:	The capability to invoke
 * sc:	 Make a composite OS system-call
 */
#define COS_SYSENTER_ENTRY					\
	asm(".globl sysenter_interposition_entry;\n\t"		\
	    ".align 4096;\n\t"					\
	    "sysenter_interposition_entry:\n\t"			\
	    "cmpl $(1<<"STR(COS_SYSCALL_OFFSET)"), %eax\n\t"	\
	    "jb linux_syscall\n\t"				\
	    SAVE_REGS_ASM					\
	    "pushl %esp\n\t"					\
	    "call composite_sysenter_dispatcher\n\t"		\
	    "addl $4, %esp\n\t"					\
	    "testl %eax, %eax\n\t"				\
	    "jne ret_from_preemption\n\t"			\
	    RESTORE_REGS_ASM					\
	    RET_TO_USER)

/* Composite sysenter entry */

#endif
