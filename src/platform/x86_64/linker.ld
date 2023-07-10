/*
 * TODO: integrate into the build system to avoid redefining the
 * constants using "gcc -P -E ldscript.c -o ldscript.ld"
 */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY (loader)

/* System constants. TODO auto-gen these (see above). */
KERN_PHYS = 0x00100000;  		/* This should be COS_MEM_KERN_PA */
KERN_VADDR = 0xffff800000000000;	/* kernel virtual address */
NUM_RETYPABLE_PAGES = 16384;	        /* 64MB memory to start */
COS_PAGE_SIZE = 4096;

SECTIONS {

    /*
     * Code and data to be linked into physical addresses. This code's
     * main job is to simply activate the page-tables and virtual addresses
     * of the kernel.
     */
    . = KERN_PHYS;

    .boottext : AT(KERN_PHYS) ALIGN(8)
    {
       loader.o(.text)
       . = ALIGN(4096);
    }

    KERN_PHYS = .;

    /*
     * The rest of the linker script creates virtual addresses that
     * must be accessed through the page-table. Specifically, these
     * virtual addresses are in the kernel portion of the virtual
     * address space.
     */
    . += KERN_VADDR;		/* This should be COS_MEM_KERN_START_VA + COS_MEM_KERN_PA */

    kernel_start_va = .;			/* The first virtual address of the kernel. */
    .text : AT(KERN_PHYS) {
        code = .;
        loader.o(.smptext)
        *(.text)
	. = ALIGN(4096);
        *(.ipc_fastpath)
        . = ALIGN(4096);
        *(.rodata)
        . = ALIGN(4096);
    }

    .data : AT(KERN_PHYS + (data - code)) {
        data = .;
        *(.data)
        . = ALIGN(4096);
    }

    .bss : AT(KERN_PHYS + (bss - code)) {
        bss = .;
        *(.bss)
	*(COMMON)
        . = ALIGN(4096);
    }

    /* The `pages` array includes all of the retypable pages in the system. */
    pages = .;
    /* The first page is a nil captbl node, then the initial image. */
    . = . + COS_PAGE_SIZE;

    /* Initial image is in pages that will be directly mapped into the constructor */
    initial_component_start = .;
    .initial_component : AT(KERN_PHYS + (initial_component - code)) {
        *(.initial_component)
    }
    . = ALIGN(4096);
    initial_component_end = .;

    /* Move to the end of the retypable page array */
    . = . + ((NUM_RETYPABLE_PAGES * COS_PAGE_SIZE) - (COS_PAGE_SIZE + initial_component_end - initial_component_start));
    kernel_end_va = .;

    /DISCARD/ : {
        *(.comment)
        *(.note.gnu.build-id)
    }
}
