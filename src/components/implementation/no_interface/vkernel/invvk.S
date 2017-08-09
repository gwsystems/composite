#define __ASM__
#include <consts.h>
#include <cos_asm_simple_stacks.h>

#ifndef RET_CAP
#define RET_CAP (1 << 16)
#endif

.text
.globl __inv_vkernel_serverfn 
.type  __inv_vkernel_serverfn, @function
__inv_vkernel_serverfn:
	COS_ASM_GET_STACK

	pushl %ebp /* arg 4 */
        pushl %edi /* arg 3 */
        pushl %esi /* arg 2 */
        pushl %ebx /* arg 1 */
	call vkernel_server 
        movl %eax, %ecx

	COS_ASM_RET_STACK
	movl $RET_CAP, %eax
	sysenter;
