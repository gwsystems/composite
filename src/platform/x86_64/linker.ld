/*
 * TODO: integrate into the build system to avoid redefining the
 * constants using "gcc -P -E ldscript.c -o ldscript.ld"
 */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY (loader)
phys = 0x00100000;  		/* This should be COS_MEM_KERN_PA */
SECTIONS {

    . = phys;

    .boottext : AT(phys) ALIGN(8)
    {
       loader.o(.text)
       . = ALIGN(4096);
    }

    phys = .;

    . += 0xffff800000000000;		/* This should be COS_MEM_KERN_START_VA + COS_MEM_KERN_PA */

    .text : AT(phys) {
        code = .;
        loader.o(.smptext)
        *(.text)
	. = ALIGN(4096);
        *(.ipc_fastpath)
        . = ALIGN(4096);
        *(.rodata)
        . = ALIGN(4096);
    }

    .data : AT(phys + (data - code)) {
        data = .;
        *(.data)
        . = ALIGN(4096);
    }

    .bss : AT(phys + (bss - code)) {
        bss = .;
        *(.bss)
	*(COMMON)
        . = ALIGN(4096);
    }

    /* The first page is a nil captbl node, then the initial image. */
    pages = .;
    . = . + 4096;

    /* /\* */
    /*  * We allocate all large allocations after the normal .bss and .data so that data */
    /*  * that is needed in the intial kernel image (referred to by a single 4MB super-page) */
    /*  *\/ */
    /* .largebss : AT(phys + (largebss - code)) { */
    /*     largebss = .; */
    /*     *(.largebss) */
    /*     . = ALIGN(4096); */
    /* } */
    /* end = .;		/\* Unfortunately, this variable will take up the whole page. *\/ */
    /* . = ALIGN(4096); */

    /* Initial image is in pages that will be directly mapped into the constructor */
    .initial_component : AT(phys + (initial_component - code)) {
        initial_component_start = .;
        *(.initial_component)
	. = ALIGN(4096);
	initial_component_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note.gnu.build-id)
    }
}
