#define __ASM__
#include <consts.h>
#include <cos_asm_simple_stacks.h>

#ifndef RET_CAP
#define RET_CAP (1 << 16)
#endif

.text
.globl __inv_vkernel_hypercallfn 
.type  __inv_vkernel_hypercallfn, @function
__inv_vkernel_hypercallfn:
	COS_ASM_GET_STACK

	pushl %ebp /* arg 4 */
	pushl %edi /* arg 3 */
	pushl %esi /* arg 2 */
	pushl %ebx /* arg 1 */
	call vkernel_hypercall 
	movl %eax, %ecx

	COS_ASM_RET_STACK
	movl $RET_CAP, %eax
	sysenter;
