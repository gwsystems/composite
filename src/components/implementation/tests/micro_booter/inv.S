/* Use the passed in stack in arg4 */

#define RET_CAP (1 << 16)

/* Initialize the bss section and data section */
    .section            .text
    .syntax             divided
    .code               32
    .align              16
    .global             __bss_start__
    .global             __bss_end__
    .global             __va_offset__
/*
    ldr                 r0,=__bss_start__
    ldr                 r1,=__bss_end__
    ldr                 r2,=__va_offset__
    sub                 r0,r0,r2
    sub                 r1,r1,r2
    ldr                 r2,=0x00
clear_bss:
    cmp                 r0,r1
    beq                 clear_done
    str                 r2,[r0]
    add                 r0,#0x04
    b                   clear_bss
clear_done:
*/
/* Find ourselves a stack to run on - the stack starts at 0x14000000. The storage is: (low to high): tls, tid, cpuid */
_start:
    .global             _start
    ldr			r12,=0x14000000
    /* The stack top address now confirmed */
    lsl                 r5,r1,#16
    lsr                 r5,r5,#3
    add			sp,r12,r5
    /* Save the cpuid */
    sub			sp,sp,#0x04
    lsr                 r5,r1,#16
    str                 r5,[sp]
    /* Save the tid */
    sub			sp,sp,#0x04
    lsl                 r5,r1,#16
    lsr                 r5,r5,#16
    str			r5,[sp]
    /* Clean up the tls */
    sub			sp,sp,$0x04
    mov			r1,#0x00
    str			r1,[sp]
    /* Point SP to somewhere safe */
    sub                 sp,sp,#0xF4
    /* Pass the parameters */
    mov                 r0,#0x00
    mov			r1,r2
    mov			r2,r3
    mov			r3,r4
    /* branch to main function */
    ldr                 r12,=cos_upcall_fn
    bx                  r12
    .ltorg

get_cur_sp:
    .global		get_cur_sp
    mov			r0,sp
    bx			lr

.text
.globl __inv_test_serverfn
__inv_test_serverfn:
	B .
