#include <chal/shared/cos_config.h>
#include "chal_asm_inc.h"
#include "multiboot2.h"

.section .text
.align 8
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

.globl loader
loader:
	mov 	$(stack + STACKSIZE - STK_INFO_OFF), %esp
	movl	%eax, %ecx
	movl    %cr4, %eax
	orl     $((1<<4) | (1<<7)), %eax
	movl    %eax, %cr4
	movl    $(boot_comp_pgd-COS_MEM_KERN_START_VA), %eax
	movl    %eax, %cr3

	# Turn on paging.
	movl    %cr0, %eax
	orl     $(1<<31), %eax
	movl    %eax, %cr0
	cli
	#pushl %esp
	movl $COS_MEM_KERN_START_VA, %eax
	add %eax, %ebx
	pushl %ecx
	pushl %ebx

	pushl $0  /* empty return value as we're jmping, not calling */
	mov $kmain, %eax
	jmp *%eax

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

	/* Turn on page size extension for 4Mbyte pages */
	movl    %cr4, %eax
	orl     $((1<<4) | (1<<7)), %eax
	movl    %eax, %cr4

	movl    $(boot_ap_pgd - COS_MEM_KERN_START_VA), %eax
	movl    %eax, %cr3

	/* Turn on paging. */
	movl    %cr0, %eax
	orl     $(CR0_PE|CR0_PG|CR0_WP), %eax
	movl    %eax, %cr0

	/* Switch to the stack allocated by startothers() */
	movl    $RELOCATE_ADDR(smpstack), %esp
	movl    (%esp), %esp
	pushl   $spin

	mov     $smp_kmain, %eax
	jmp     *%eax
spin:
	jmp     spin

# setup an initial gdt that will be overridden in C code
.p2align 2
smpgdt:
  SEG_NULLASM
  SEG_ASM(STA_X|STA_R, 0, 0xffffffff)
  SEG_ASM(STA_W, 0, 0xffffffff)

smpgdtdesc:
  .word   (smpgdtdesc - smpgdt - 1)
  .long   RELOCATE_ADDR(smpgdt)

.global smpstack
smpstack:
.long 0xDEADBEEF

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
