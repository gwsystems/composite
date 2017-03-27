/* 
 * TODO: integrate into the build system to avoid redefining the
 * constants using "gcc -P -E ldscript.c -o ldscript.ld" 
 */

OUTPUT_FORMAT(elf32-i386)
ENTRY (loader)
phys = 0x00100000;  		/* This should be COS_MEM_KERN_PA */
SECTIONS {

    . = 0xc0100000;		/* This should be COS_MEM_KERN_START_VA + COS_MEM_KERN_PA */
    
    .text : AT(phys) {
        code = .;
        *(.multiboot)
        *(.text)
        *(.rodata)
        . = ALIGN(4096);
        *(__ipc_entry)
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
	. = ALIGN(4096);
    }

    /* 
     * We allocate all large allocations after the normal .bss and .data so that data 
     * that is needed in the intial kernel image (referred to by a single 4MB super-page) 
     */
    .largebss : AT(phys + (largebss - code)) {
    	largebss = .;
        *(.largebss)
    }

    . = ALIGN(4096); 	/* This should be RETYPE_MEM_SIZE, boot component follows... */
    end = .;		/* Unfortunately, this variable will take up the whole page. */

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}
