/* test 1:	 162-166 cycles 
.text
.globl call_asym_stub
.align 4
call_asym_stub:
	movl $65537, %eax
	pushl %ecx
	pushl %edx
	pushl %ebp
	movl %esp, %ecx
	movl $ret_asym_stub, %edx
	sysenter
.align 4
ret_asym_stub:
	popl %ebp
	popl %edx
	popl %ecx
	ret
*/


/* test 2: 312 cycles */
/*
.text
.globl call_asym_stub
.align 4
call_asym_stub:
	movl $65537, %eax
	pushl %ecx
	pushl %edx
	movl %esp, %ecx
	movl $ret_asym_stub, %edx
	sysenter
.align 4
ret_asym_stub:
	popl %edx
	popl %ecx
	ret

.globl recv_asym_stub
.align 4
recv_asym_stub:
	movl $65536, %eax
	sysenter

With the following in the kernel:

.text
.globl composite_call_ipc
.align 4
composite_call_ipc:
	movl %edx, saved_ip
	movl %ecx, saved_sp
	movl stub_addr, %edx
	movl jmp_addr, %eax
	sysexit

.globl composite_ret_ipc
.align 4
composite_ret_ipc:
	movl saved_ip, %edx
	movl saved_sp, %ecx
	sysexit

*/

/* test 3: 352 cycles
.text
.globl call_asym_stub
.align 4
call_asym_stub:
	movl 0x0(%esp), %eax
	pushl %ecx
	pushl %edx
	pushl %esi
	movl %esp, %ecx
	movl $call_ret_asym_stub, %edx
	movl %eax, %esi
	movl $65537, %eax
	sysenter
.align 4
call_ret_asym_stub:
	movl %esi, 12(%esp)
	popl %esi
	popl %edx
	popl %ecx
	ret

.globl recv_asym_stub
.align 4
recv_asym_stub:
	popl %esi
	popl %edx
	popl %ecx
	movl $recv_ret_asym_stub, 0x0(%esp)
	jmp *%eax
.align 4
recv_ret_asym_stub:
	pushl %ecx
	pushl %edx
	pushl %esi
	movl %eax, %esi
	movl $65536, %eax
	sysenter

With the following kernel code:
	.text
.globl composite_call_ipc
.align 4
composite_call_ipc:
	movl %edx, saved_ip
	movl %ecx, saved_sp
	movl %esi, saved_ret_ip
	movl stub_addr, %edx
	movl jmp_addr, %eax
	sysexit

.globl composite_ret_ipc
.align 4
composite_ret_ipc:
	movl %esi, %eax
	movl saved_ip, %edx
	movl saved_sp, %ecx
	movl saved_ret_ip, %esi
	sysexit

*/
	
/*
The following yields 860 cycles.

.text
.globl call_asym_stub
.align 4
call_asym_stub:
	movl 0x0(%esp), %eax
	pushl %ecx
	pushl %edx
	pushl %esi
	movl %esp, %ecx
	movl $call_ret_asym_stub, %edx
	movl %eax, %esi           
	movl $65537, %eax         
	sysenter
.align 4
call_ret_asym_stub:
	movl %esi, 12(%esp) 
	popl %esi
	popl %edx
	popl %ecx
	ret

.globl recv_asym_stub
.align 4
recv_asym_stub:
	popl %esi
	popl %edx
	popl %ecx
	movl $recv_ret_asym_stub, 0x0(%esp) 
	jmp *%eax
.align 4
recv_ret_asym_stub:
	pushl %ecx
	pushl %edx
	pushl %esi
	movl %eax, %esi
	movl $65536, %eax
	sysenter	
		
With kernel:
	
.text
.globl composite_call_ipc
.align 4
composite_call_ipc:
	movl %edx, saved_ip
	movl %ecx, saved_sp
	movl %esi, saved_ret_ip
	movl saved_cr3, %edx
	movl %edx, %cr3 
	movl stub_addr, %edx
	movl jmp_addr, %eax
	sysexit

.globl composite_ret_ipc
.align 4
composite_ret_ipc:
	movl %esi, %eax 
	movl saved_ip, %edx
	movl saved_sp, %ecx
	movl saved_ret_ip, %esi
	sysexit

And with the change to the kernel part it takes 1110 cycles:

+composite_ret_ipc:
+	movl %esi, %eax 
+	movl saved_cr3, %edx
+	movl %edx, %cr3
+	movl saved_ip, %edx

IE, the overhead of writing to cr3 (alone) is 250 cycles, but
secondary effects for minimal programs are at least ~500 cycles.
	
*/	
	
.text
.globl call_asym_stub
.align 4
call_asym_stub:
	movl 0x0(%esp), %eax /* ret addr on stack */
	pushl %ecx
	pushl %edx
	pushl %esi
	movl %esp, %ecx
	movl $call_ret_asym_stub, %edx /* stub to return to */
	movl %eax, %esi           /* fn return addr (overridden in server) */
	movl $65537, %eax         /* > 2^16 = invocation */
	sysenter
.align 4
call_ret_asym_stub:
	movl %esi, 12(%esp) /* restore return address */
	popl %esi
	popl %edx /* edx/ecx trashed from sysexit */
	popl %ecx
	ret

.globl recv_asym_stub
.align 4
recv_asym_stub:
	popl %esi
	popl %edx
	popl %ecx
	movl $recv_ret_asym_stub, 0x0(%esp) /* on fn return, execute exit stub */
	jmp *%eax
.align 4
recv_ret_asym_stub:
	pushl %ecx
	pushl %edx
	pushl %esi
	movl %eax, %esi
	movl $65536, %eax /* 2^16 = ret */
	sysenter	
		

/*
Much more recent:
IPC takes 1629 cycles
Thread switch takes 529
*/
