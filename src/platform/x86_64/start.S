#include "multiboot2.h"


/* Multiboot2 header must be contained completely within the first 32768 bytes of the OS image, and must be 64-bit aligned */
.section .text
multiboot_start:

    /* Must have multiboot2 header field */
    .long MULTIBOOT2_HEADER_MAGIC
    .long MULTIBOOT_ARCHITECTURE_I386
    .long multiboot_end - multiboot_start
    .long -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot_end - multiboot_start))

    /* End tag of multiboot2 */
    .short MULTIBOOT_HEADER_TAG_END
    .short 0
    .long 8

multiboot_end:


/* 
Grub enters 32-bit mode, so your first section should use 32-bit code.
You can try to jump to 64 bit yourself then if you want.
*/
.code32
.global entry_point
entry_point:
    /* Prepare to enter long(64-bit) mode */

    /* Close paging first */
    mov %cr0, %eax                                   
    and $0b1111111111111111111111111111111, %eax   
    mov %eax, %cr0                                

    
    /* 
        In order to keep executing instruction correctly once after switching to long mode,
        We need do identity mapping, which means the virtual address is the same as the physical
        address. Thus, even if we set up new paging scheme when switching to long mode, after that
        instruction, its following instructions can still find the correct address(V_addr == P_addr) 
        to execute. Here, we map the first 10M physical address to the first 10M virtual address, because
        our kernel is loaded at this renge. Once we successfully entered long mode, which means we can visit
        64-bit range memory, we can then map higher virtual address to this physical memory range so that 
        the higher virtual address kernel part can be visited.
     */

    /* We use 2M paging scheme, so only to set P4_table, P3_table and P2_table here */
    mov $P3_table, %eax
    or $0x11, %eax
    mov %eax, P4_table

    mov $P2_table, %eax
    or $0x11, %eax
    mov %eax, P3_table

    movl  $0x000083, P2_table
	movl  $0x200083, (P2_table+0x08*1)
	movl  $0x400083, (P2_table+0x08*2)
	movl  $0x600083, (P2_table+0x08*3)
	movl  $0x800083, (P2_table+0x08*4)
	movl  $0xA00083, (P2_table+0x08*5)


    /* set kernel paging space */
    mov $P2_table_kernel, %eax
    or $0x11, %eax
    mov %eax, (P3_table + 0x08 * 3)

    movl  $0x000083, P2_table_kernel
	movl  $0x200083, (P2_table_kernel+0x08*1)
	movl  $0x400083, (P2_table_kernel+0x08*2)
	movl  $0x600083, (P2_table_kernel+0x08*3)
	movl  $0x800083, (P2_table_kernel+0x08*4)
	movl  $0xA00083, (P2_table_kernel+0x08*5)
    movl  $0xC00083, (P2_table_kernel+0x08*6)

    /* Set %cr3 to the physical address of P4_table */
    mov $P4_table, %eax
    mov %eax, %cr3

    /* Enable PAE-paging by setting the PAE-bit in %cr4 */
    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4

    /* Set the long mode bit in the EFER MSR */
    mov $0xC0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr

    /* Enable paging and protected mode in cr0 */
    mov %cr0, %eax
    or $(1 << 31), %eax
    mov %eax, %cr0

    /* Load GDT and IDT so we can use selectors within them */
    mov $(Gdt_Ptr), %ebx
	lgdt (%ebx)
	mov $(Idt_Ptr), %ebx
	lidt (%ebx)

    /* 
        We are still in identity mapping range, but now we are in long mode. 
        Thus, we can here to jmp to a higher virtual address by using ret instruction.
        At the same time, this will flush %cs selector, so the PC(program pointer) can 
        really switch to long mode. 
    */
.code64
    mov $entry_64, %rax
	pushq $0x08
	pushq %rax
	lretq


entry_64:
     
    /* Flush data selectors */
    mov $0x10, %rax	
	mov %rax, %ds
	mov %rax, %es
	mov %rax, %fs
	mov %rax, %gs
	mov %rax, %ss

    movb $'*', 0x8b000
    movb $12, 0xb8001
    movabs $kmain, %rax
	pushq $0x08
	pushq %rax
	lretq
    jmp .


/* GDT/IDT data structure */
.align 8
.global Gdt_Table
Gdt_Table:
	.quad	0x0000000000000000	// null descriptor
	.quad	0x0020980000000000	// kernel code descriptor
	.quad 	0x0000920000000000	// kernel data descriptor
Gdt_End:

Gdt_Ptr:	
		.word	Gdt_End - Gdt_Table - 1
		.quad	Gdt_Table

.align 8
.global Idt_Table
Idt_Table:
	.fill 256*2, 8, 0	// .fill repeat, size, value
Idt_End:

Idt_Ptr:
	.word	Idt_End - Idt_Table - 1
	.quad	Idt_Table	

/* Temp page directory/tables */
/* Need to be 4k aligned */
.align 4096
.global P4_table
P4_table:
	.fill 512, 8, 0
.global P3_table
P3_table:
	.fill 512, 8, 0
.global P2_table
P2_table:	
	.fill 512, 8, 0
.global P2_table_kernel
P2_table_kernel:
    .fill 512, 8, 0

