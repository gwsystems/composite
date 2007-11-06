#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

.data
/* 8 4K stacks */
.align 32
.globl nil
nil:
	.rep 1024 
	.long 0
	.endr
.globl cos_static_stack
cos_static_stack:
	.rep 8193
	.long 0
	.endr

/* Scheduler page for event notifications and thread switching. */
.align 4096
.globl cos_sched_notifications
cos_sched_notifications:
	.rep 1024
	.long 0
	.endr
	
.text
.globl cos_upcall_entry
.type  cos_upcall_entry, @function
.align 16
cos_upcall_entry:	
	movl $cos_static_stack, %esp
	movl %eax, %ebp /* thread id */
	shl $12, %ebp /* 4K stacks */
	addl %ebp, %esp

	pushl %esi
	pushl %edi
	pushl %ebx
	pushl %eax /* thread id */
	pushl %ecx /* data region ptr */	
	call cos_upcall_fn
	/* addl $8, %esp <- dont need to clean the stack */

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter
	
