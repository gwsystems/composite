OUTPUT_FORMAT(elf32-i386)
ENTRY (loader)
phys = 0x00100000; 
SECTIONS {
    
    .text phys : AT(phys) {
        code = .; _code = .; __code = .;
        *(.text)
        *(.rodata)
        . = ALIGN(4096);
    }

    .data : AT(phys + (data - code)) {
        data = .; _data = .; __data = .;
        *(.data)
        . = ALIGN(4096);
    }

    .bss : AT(phys + (bss - code)) {
        bss = .; _bss = .; __bss = .;
        *(.bss)
        . = ALIGN(4096);
    }

    end = .; _end = .; __end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.gnu.build-id)
    }
}

/*
*SECTIONS
*{
*    . = 0x00100000;
*
*    .text ALIGN (0x1000) :
*    {
*        *(.text)
*    }
*
*    .rodata ALIGN (0x1000) :
*    {
*        *(.rodata*)
*    }
*
*    .data ALIGN (0x1000) :
*    {
*        *(.data)
*    }
*
*    .bss :
*    {
*        sbss = .;
*        *(COMMON)
*        *(.bss)
*        ebss = .;
*    }
*}
**/
