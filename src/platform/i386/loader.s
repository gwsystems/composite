.global loader

.set MULTIBOOT_PAGE_ALIGN,  1<<0
.set MULTIBOOT_MEMINFO,     1<<1
.set MULTIBOOT_AOUT_KLUDGE, 1<<16
.set MULTIBOOT_MAGIC,       0x1BADB002
.set MULTIBOOT_FLAGS,       MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMINFO
.set MULTIBOOT_CHECKSUM,    -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

.align 4
.long MULTIBOOT_MAGIC
.long MULTIBOOT_FLAGS
.long MULTIBOOT_CHECKSUM

.set STACKSIZE, 0x4000
.comm stack, STACKSIZE, 32

loader:
    mov $(stack + STACKSIZE), %esp
    push %esp
    push %eax
    push %ebx

    cli
    call kmain

hang:
    hlt
    jmp hang
