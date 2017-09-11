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

.text
.globl __inv_rk_inv_entry
.type  __inv_rk_inv_entry, @function
__inv_rk_inv_entry:
	COS_ASM_GET_STACK

        pushl %ebp /* arg 4 */
        pushl %edi /* arg 3 */
        pushl %esi /* arg 2 */
        pushl %ebx /* arg 1 */
        call rk_inv_entry
        movl %eax, %ecx

	COS_ASM_RET_STACK
        movl $RET_CAP, %eax
        sysenter

