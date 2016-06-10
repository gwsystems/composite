#define RET_CAP (1 << 16)

#define vcos_stub(name, fn) \
.globl name##_inv ;                     \
.type  name##_inv, @function ;          \
name##_inv:                             \
	movl %ebp, %esp;                \
	xor %ebp, %ebp;                 \
        pushl %edi;                     \
        pushl %esi;                     \
        pushl %ebx;                     \
        call fn ; 		        \
        addl $12, %esp;                 \
        movl %eax, %ecx;                \
        movl $RET_CAP, %eax;            \
        sysenter;			\

.text
vcos_stub(vm_captbl_op, __vcos_captbl_op)

