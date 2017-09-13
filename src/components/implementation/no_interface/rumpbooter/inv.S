#define __ASM__
#include <consts.h>
#include <cos_asm_simple_stacks.h>

#ifndef RET_CAP
#define RET_CAP (1 << 16)
#endif

.text
.globl __inv_test_serverfn
.type  __inv_test_serverfn, @function
__inv_test_serverfn:
	movl %ebp, %esp
	xor %ebp, %ebp
        pushl %edi
        pushl %esi
        pushl %ebx
        call test_serverfn
        addl $12, %esp
        movl %eax, %ecx
        movl $RET_CAP, %eax
        sysenter;

#define INV_ENTRY(fn)			\
.globl __inv_##fn;			\
.type  __inv_##fn, @function;		\
__inv_##fn:				\
	COS_ASM_GET_STACK;		\
        pushl %ebp;			\
        pushl %edi; 		        \
        pushl %esi; 		        \
        pushl %ebx; 		        \
        call fn;			\
        movl %eax, %ecx;		\
	COS_ASM_RET_STACK;		\
        movl $RET_CAP, %eax;		\
        sysenter

.text
INV_ENTRY(vkernel_hypercall)
INV_ENTRY(rk_inv_entry)
INV_ENTRY(timer_inv_entry)
