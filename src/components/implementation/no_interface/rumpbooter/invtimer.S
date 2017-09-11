#define __ASM__
#include <consts.h>
#include <cos_asm_simple_stacks.h>

#ifndef RET_CAP
#define RET_CAP (1 << 16)
#endif

.text
.globl __inv_timer_comp_invfn
.type  __inv_timer_comp_invfn, @function
__inv_timer_comp_invfn:
        COS_ASM_GET_STACK

        call timer_comp_inv 
        movl %eax, %ecx

        COS_ASM_RET_STACK
        movl $RET_CAP, %eax
        sysenter;

