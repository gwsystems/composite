#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning (through 
   ipc_server_ret_marshal) */
.globl spd1_inv
.type  spd1_inv, @function
.align 16
spd1_inv:
	movl %ebp, %esp
	/* pops here to support stack sharing */
/*	popl %edx
	popl %ecx*/
	popl %ebp
	addl $4, %esp /* get rid of the ret addr */
/*	call *%eax*/
	call spd1_fn
	pushl $0 /* space for the retaddr */
	pushl %ebp
/*	pushl %ecx
	pushl %edx*/
	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.data


.rep 256
.long 4
.endr
