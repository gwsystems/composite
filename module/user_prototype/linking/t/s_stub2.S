#include <cos_asm_stacks.h>
	
#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

.text	
/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning (through 
   ipc_server_ret_marshal) */
.globl spd2_fn_inv
.type  spd2_fn_inv, @function
.align 16
spd2_fn_inv:
	COS_ASM_GET_STACK
	
/*	movl %ebp, %esp */
/*	popl %ebp*/
	call spd2_fn
/*	pushl %ebp*/

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl yield_inv
.type  yield_inv, @function
.align 16
yield_inv:
	COS_ASM_GET_STACK

/*	movl %ebp, %esp */
/*	popl %ebp*/
	call yield
/*	pushl %ebp*/

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl nothing_inv
.type  nothing_inv, @function
.align 16
nothing_inv:
	COS_ASM_GET_STACK
	
/*	movl %ebp, %esp */
/*	popl %ebp*/
	call nothing
/*	pushl %ebp*/

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

	
.globl sched_init_inv
.type  sched_init_inv, @function
.align 16
sched_init_inv:
	COS_ASM_GET_STACK
	
/*	movl %ebp, %esp */
/*	popl %ebp*/
	call sched_init
/*	pushl %ebp*/

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter
