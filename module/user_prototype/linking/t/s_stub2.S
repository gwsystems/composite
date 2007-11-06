#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

.text	
/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning (through 
   ipc_server_ret_marshal) */
.globl spd2_inv
.type  spd2_inv, @function
.align 16
spd2_inv:
	movl $cos_static_stack, %esp
	movl %eax, %ebp
	shl $12, %ebp /* 4K stacks */
	addl %ebp, %esp
	
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
	movl $cos_static_stack, %esp
	movl %eax, %ebp
	shl $12, %ebp /* 4K stacks */
	addl %ebp, %esp

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
	movl $cos_static_stack, %esp
	movl %eax, %ebp
	shl $12, %ebp /* 4K stacks */
	addl %ebp, %esp
	
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
	movl $cos_static_stack, %esp
	movl %eax, %ebp
	shl $12, %ebp /* 4K stacks */
	addl %ebp, %esp
	
/*	movl %ebp, %esp */
/*	popl %ebp*/
	call sched_init
/*	pushl %ebp*/

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter
