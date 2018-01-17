#define __ASM__
#include <consts.h>
#include <cos_asm_simple_stacks.h>

#ifndef RET_CAP
#define RET_CAP (1 << 16)
#endif

#define INV_ENTRY(fn)			\
.globl __inv_##fn;			\
.type  __inv_##fn, @function;		\
__inv_##fn:				\
	COS_ASM_GET_STACK;		\
        pushl %ebp;			\
        pushl %edi;		        \
        pushl %esi;		        \
        pushl %ebx;		        \
        call fn;			\
        movl %eax, %ecx;		\
	COS_ASM_RET_STACK;		\
        movl $RET_CAP, %eax;		\
        sysenter

.text
//INV_ENTRY(vkernel_hypercall)
//INV_ENTRY(rk_inv_entry)
//INV_ENTRY(timer_inv_entry)
