/* Use the passed in stack in arg4 */

#include <cos_asm_simple_stacks.h>

#define RET_CAP (1 << 16)

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
.globl __inv_test_entry
.type  __inv_test_entry, @function
__inv_test_entry:
	COS_ASM_GET_STACK
	pushl %ebp
	xor %ebp, %ebp
        pushl %edi
        pushl %esi
        pushl %ebx
        call test_entry
        addl $16, %esp
        movl %eax, %ecx
        movl $RET_CAP, %eax
	COS_ASM_RET_STACK
        sysenter;
