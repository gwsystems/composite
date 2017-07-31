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

        pushl %ebp /* arg 4 */
        pushl %edi /* arg 3 */
        pushl %esi /* arg 2 */
        pushl %ebx /* arg 1 */
        call test_entry
        movl %eax, %ecx

	COS_ASM_RET_STACK
        movl $RET_CAP, %eax
        sysenter;

.text
.globl __inv_test_shdmem
.type  __inv_test_shdmem, @function
__inv_test_shdmem:
	COS_ASM_GET_STACK

        pushl %ebp /* arg 4 */
        pushl %edi /* arg 3 */
        pushl %esi /* arg 2 */
        pushl %ebx /* arg 1 */
        call test_shdmem
        movl %eax, %ecx

	COS_ASM_RET_STACK
        movl $RET_CAP, %eax
        sysenter

.text
.globl __inv_test_fs
.type  __inv_test_fs, @function
__inv_test_fs:
	COS_ASM_GET_STACK

        pushl %ebp /* arg 4 */
        pushl %edi /* arg 3 */
        pushl %esi /* arg 2 */
        pushl %ebx /* arg 1 */
        call test_fs
        movl %eax, %ecx

	COS_ASM_RET_STACK
        movl $RET_CAP, %eax
        sysenter;

.text
.globl __inv_shdmem_get_vaddr
.type  __inv_shdmem_get_vaddr, @function
__inv_shdmem_get_vaddr:
	COS_ASM_GET_STACK

        pushl %ebp /* arg 4 */
        pushl %edi /* arg 3 */
        pushl %esi /* arg 2 */
        pushl %ebx /* arg 1 */
        call shdmem_get_vaddr
        movl %eax, %ecx

	COS_ASM_RET_STACK
        movl $RET_CAP, %eax
        sysenter;


.text
.globl __inv_shdmem_allocate
.type  __inv_shdmem_allocate, @function
__inv_shdmem_allocate:
	COS_ASM_GET_STACK

        pushl %ebp /* arg 4 */
        pushl %edi /* arg 3 */
        pushl %esi /* arg 2 */
        pushl %ebx /* arg 1 */
        call shdmem_allocate
        movl %eax, %ecx

	COS_ASM_RET_STACK
        movl $RET_CAP, %eax
        sysenter;

.text
.globl __inv_shdmem_deallocate
.type  __inv_shdmem_deallocate, @function
__inv_shdmem_deallocate:
	COS_ASM_GET_STACK

        pushl %ebp /* arg 4 */
        pushl %edi /* arg 3 */
        pushl %esi /* arg 2 */
        pushl %ebx /* arg 1 */
        call shdmem_deallocate
        movl %eax, %ecx

	COS_ASM_RET_STACK
        movl $RET_CAP, %eax
        sysenter;

.text
.globl __inv_shdmem_map
.type  __inv_shdmem_map, @function
__inv_shdmem_map:
	COS_ASM_GET_STACK

        pushl %ebp /* arg 4 */
        pushl %edi /* arg 3 */
        pushl %esi /* arg 2 */
        pushl %ebx /* arg 1 */
        call shdmem_map
        movl %eax, %ecx

	COS_ASM_RET_STACK
        movl $RET_CAP, %eax
        sysenter;
