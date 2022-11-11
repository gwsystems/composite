#include "chal/shared/cos_config.h"
#include "chal_asm_inc.h"
#include "multiboot2.h"

/* multiboot2 header must be contained completely within the first 32768 bytes of the OS image, and must be 64-bit aligned */
.section .text
multiboot_start:
	/* must have multiboot2 header field */
	.long MULTIBOOT2_HEADER_MAGIC
	.long MULTIBOOT_ARCHITECTURE_I386
	.long multiboot_end - multiboot_start
	.long -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot_end - multiboot_start))

	/* end tag of multiboot2 */
	.short MULTIBOOT_HEADER_TAG_END
	.short 0
	.long 8

multiboot_end:

/* 
 * grub enters 32-bit mode, so your first section should use 32-bit code.
 * jmp to 64 bit yourself from here.
 */
.code32
.global loader 
loader:
	/* save multiboot2 args first */
	mov %eax, multiboot_magic
	mov %ebx, multiboot_tags

	/* repare to enter long(64-bit) mode */

	/* close paging first */
	mov %cr0, %eax
	and $(~(1<<31)), %eax
	mov %eax, %cr0

	/* test if 1G huge page is supported */
	/* some cpus don't support 1G huge page, we enable this by set qemu -cpu max */
	mov $0x80000001, %eax
	cpuid
	bt $26, %edx
	jc huge_page_enabled
	/* TODO: support for cpus which don't have 1G paging scheme here */
	jmp .

huge_page_enabled:
	/* we have to set up a temparaily page scheme to before enter long mode */
	/* we use 1G paging scheme, so only to set p0_pgt, p1_pgt */
	mov $p1_pgt, %eax
	or $0x03, %eax
	mov %eax, p0_pgt
	mov %eax, (p0_pgt+0x08*256)
	
	/* set kernel paging space */
	movl  $0x00000083, p1_pgt /* 0-1 GB vm -> 0-1 GB pm */

	/* set %cr3 to the physical address of p0_pgt */
	mov $p0_pgt, %eax
	mov %eax, %cr3

	/* enable PAE-paging by setting the PAE-bit in %cr4 */
	mov %cr4, %eax
	or $(1 << 5), %eax
	mov %eax, %cr4

	/* set the long mode bit in the EFER MSR */
	mov $0xC0000080, %ecx
	rdmsr
	or $(1 << 8), %eax
	wrmsr

	/* enable paging and protected mode in cr0 */
	mov %cr0, %eax
	or $(1 << 31), %eax
	mov %eax, %cr0

	/* load GDT so we can use selectors within them */
	mov $gdt_ptr, %eax
	lgdt (%eax)

	/* need a jump to flush cs selector */
.code64
	mov $entry_64, %rax
	pushq $0x08
	pushq %rax
	lretq


entry_64:
	cli

	/* flush data selectors */
	mov $0x10, %rax
	mov %rax, %ds
	mov %rax, %es
	mov %rax, %fs
	mov %rax, %gs
	mov %rax, %ss

	/* set up a kernel stack for kmain */
	movabs $(stack + STACKSIZE - STK_INFO_OFF), %rsp
	xor %rsi, %rsi
	xor %rdi, %rdi
	movl multiboot_magic, %esi
	movl multiboot_tags, %edi
	movabs $COS_MEM_KERN_START_VA, %rax
	add %rax, %rdi

	movabs $kmain, %rax
	pushq $0x08
	pushq %rax
	lretq
	jmp .

.align 8
.global multiboot_magic
multiboot_magic:
	.fill 1, 4, 0
.global multiboot_tags
multiboot_tags:
	.fill 1, 4, 0

/* a simple temporary GDT */
.align 8
gdt_table:
	.quad	0x0000000000000000	// null descriptor
	.quad	0x0020980000000000	// kernel code descriptor
	.quad	0x0000920000000000	// kernel data descriptor
gdt_end:

.global gdt_ptr
gdt_ptr:
	.word	gdt_end - gdt_table - 1
	.quad	gdt_table

/* temporary page directory/tables */
/* need to be 4k aligned */
.align 4096
/* P0 is PGD */
.global p0_pgt
p0_pgt:
	.fill 512, 8, 0

.global p1_pgt
p1_pgt:
	.fill 512, 8, 0

.section .data
.set STACKSIZE, 0x1000
.global stack
.align 0x1000
stack:
.fill (STACKSIZE * NUM_CPU), 1, 0

.section .smptext

/***
The following SMP boot code is from xv6, thus:

Copyright (c) 2006-2009 Frans Kaashoek, Robert Morris, Russ Cox,
                        Massachusetts Institute of Technology

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
***/

#define SEG_NULLASM                                             \
        .word 0, 0;                                             \
        .byte 0, 0, 0, 0

// The 0xC0 means the limit is in 4096-byte units
// and (for executable segments) 32-bit mode.
#define SEG_ASM(type,base,lim)                                  \
        .word (((lim) >> 12) & 0xffff), ((base) & 0xffff);      \
        .byte (((base) >> 16) & 0xff), (0x90 | (type)),         \
                (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#define STA_X     0x8       // Executable segment
#define STA_E     0x4       // Expand down (non-executable segments)
#define STA_C     0x4       // Conforming code segment (executable only)
#define STA_W     0x2       // Writeable (non-executable segments)
#define STA_R     0x2       // Readable (executable segments)
#define STA_A     0x1       // Accessed

#define CR0_PE          0x00000001      // Protection Enable
#define CR0_WP          0x00010000      // Write Protect
#define CR0_PG          0x80000000      // Paging

/*
 * All of this code is compiled as part of the kernel, but will be
 * dynamically copied into SMP_BOOT_PATCH_ADDR.  Given that the code
 * is linked to another set of addresses (in the kernel), we need
 * to manually relocate many of the addresses here.  Thus, these
 * macros.
 */
#define RELOCATE_ADDR(a)    a - smppatchstart + SMP_BOOT_PATCH_ADDR
/* when the code segment is = SMP_BOOT_PATCH_ADDR, use this: */
#define RELOCATE_ADDR_CS(a) a - smppatchstart

.align 4096
.code16
.globl smppatchstart
smppatchstart:
.globl smploader
smploader:
	cli

	movw    %cs, %ax
	movw    %ax, %ds
	lgdt    RELOCATE_ADDR_CS(smpgdtdesc) /* using cs as ds: idea taken from Quest */
	xorw    %ax, %ax
	movw    %ax, %ds

	movl    %cr0, %eax
	orl     $CR0_PE, %eax
	movl    %eax, %cr0

	ljmpl    $(SEL_KCSEG), $RELOCATE_ADDR(smpstart32)

.align 0x10
.code32
smpstart32:
	movw    $(SEL_KDSEG), %ax
	movw    %ax, %ds
	movw    %ax, %es
	movw    %ax, %ss
	movw    $0, %ax
	movw    %ax, %fs
	movw    %ax, %gs

	mov %cr0, %eax
	and $0xFFFB, %ax	
	or $0x2, %ax	
	mov %eax, %cr0
	mov %cr4, %eax
	or $(3 << 9), %ax	
	mov %eax, %cr4

	mov %cr0, %eax
	and $(~(1<<31)), %eax
	mov %eax, %cr0

	/* test if 1G huge page is supported */
	mov $0x80000001, %eax
	cpuid
	bt $26, %edx
	jc smp_huge_page_enabled
	/* TODO: support for cpus which don't have 1G paging scheme here */
	jmp .

smp_huge_page_enabled:
	movl    $p0_pgt, %eax
	movl    %eax, %cr3

	/* enable PAE-paging by setting the PAE-bit in %cr4 */
	mov %cr4, %eax
	or $(1 << 5), %eax
	mov %eax, %cr4

	/* set the long mode bit in the EFER MSR */
	mov $0xC0000080, %ecx
	rdmsr
	or $(1 << 8), %eax
	wrmsr

	/* enable paging and protected mode in cr0 */
	mov %cr0, %eax
	or $(1 << 31), %eax
	mov %eax, %cr0

	/* load GDT so we can use selectors within them */
	mov $gdt_ptr, %eax
	lgdt (%eax)

.code64
	mov $RELOCATE_ADDR(smpstack_end) ,%rsp 
	mov $RELOCATE_ADDR(smp_entry_64), %rax
	pushq $0x08
	pushq %rax
	lretq

smp_entry_64:
	/* flush data selectors */
	mov $0x10, %rax	
	mov %rax, %ds
	mov %rax, %es
	mov %rax, %fs
	mov %rax, %gs
	mov %rax, %ss

	/* set up a kernel stack for smp_kmain */
	movq    $RELOCATE_ADDR(smpstack), %rsp
	movq    (%rsp), %rsp
	xor %rsi, %rsi
	xor %rdi, %rdi
	movl multiboot_magic, %esi
	movl multiboot_tags, %edi
	movabs $COS_MEM_KERN_START_VA, %rax
	add %rax, %rdi

	movabs $smp_kmain, %rax
	pushq $0x08
	pushq %rax
	lretq
	jmp .

# setup an initial gdt that will be overridden in C code
.p2align 2
smpgdt:
  SEG_NULLASM
  SEG_ASM(STA_X|STA_R, 0, 0xffffffff)
  SEG_ASM(STA_W, 0, 0xffffffff)

smpgdtdesc:
  .word   (smpgdtdesc - smpgdt - 1)
  .long   RELOCATE_ADDR(smpgdt)

.align 8
.global smpstack
smpstack:
.quad 0xDEADBEEF
.fill 9, 8 ,0
smpstack_end:

.globl smppatchend
smppatchend:
	nop

/*
http://wiki.osdev.org/Shutdown

http://stackoverflow.com/questions/21463908/x86-instructions-to-power-off-computer-in-real-mode
http://stackoverflow.com/questions/678458/shutdown-the-computer-using-assembly
http://stackoverflow.com/questions/3145569/how-to-power-down-the-computer-from-a-freestanding-environment
*/

.code16
.globl shutdown_apm
shutdown_apm:
mov $0x5301, %ax
xor %bx, %bx
int $0x15

/* Try to set apm version (to 1.2). */
mov $0x530e, %ax
xor %bx, %bx
mov $0x0102, %cx
int $0x15

/* Turn off the system. */
mov $0x5307, %ax
mov $0x0001, %bx
mov $0x0003, %cx
int $0x15