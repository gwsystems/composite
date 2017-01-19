#include "chal_asm_inc.h"

.syntax unified
/* Composite pt_regs save/restore */
/*
#define SAVE_REGS_GENERAL 	\
	pushl %eax;		\
	pushl %ebp;		\
	pushl %edi;		\
	pushl %esi;		\
	pushl %edx;		\
	pushl %ecx;		\
	pushl %ebx*/

#define SAVE_REGS_GENERAL push.w {r4-r11,lr}

/*
#define SAVE_REGS_ALL 		\
	subl $40, %esp; 	\
	SAVE_REGS_GENERAL*/

#define SAVE_REGS_ALL 		\
	sub sp,#0x40; 	\
	SAVE_REGS_GENERAL

/* When return to user: ecx => sp, edx => ip. */
/*
#define RESTORE_REGS_GENERAL	\
	popl %ebx;		\
	popl %ecx;		\
	popl %edx;		\
	popl %esi;		\
	popl %edi;		\
	popl %ebp;		\
	popl %eax*/

#define RESTORE_REGS_GENERAL pop.w {r4-r11,lr}

/*
#define RESTORE_REGS_ALL	\
	RESTORE_REGS_GENERAL; 	\
	addl $40, %esp*/

#define RESTORE_REGS_ALL	\
	RESTORE_REGS_GENERAL; 	\
	add sp, #0x40

/*
#define RET_TO_USER \
	sti; 	    \
	sysexit */
#define RET_TO_USER cpsie i;

/* The entire IPC path should be in __ipc_entry section. */
/* There should be a vector. Should this directly be the interrupt handler? */
/*.section __ipc_entry, "ax"
.align 16
.globl sysenter_entry
sysenter_entry:
	SAVE_REGS_ALL

	pushl %esp *//* pt_reg */
	/*call composite_syscall_handler
	addl $4, %esp

	testl %eax, %eax
	jne restore_from_thd

	RESTORE_REGS_ALL
	RET_TO_USER*/

/* .section .after_vectors */
/* .align 4 */
/* .globl SVC_Handler This is not currently used */
SVC_Handler:
    nop
    nop
    nop
    nop
	SAVE_REGS_ALL

	/* push {sp} */ /* pt_reg */
	bl composite_syscall_handler
	/* add sp, #0x04 */
	tst r0, r0
	it ne
	blne restore_from_thd

	RESTORE_REGS_ALL
	RET_TO_USER


/* we are changing thread contexts, reload all registers */
.align 16
restore_from_thd:
	RESTORE_REGS_GENERAL
	/* addl $4, %esp */
	cpsie i
	bx lr
