OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

PROVIDE(__va_offset__ = 0x80000000);
phys = 0x00100000;
SECTIONS
{
  /* the entry point, before enabling paging. The code to enable paing
   needs to have the same virtual/physical address. entry.S and start.c
   run in this initial setting.*/
  . = 0x80100000;

  .text : AT(phys) {
    code = .;
    *start.o(.text .rela.text .stub .text.* .gnu.linkonce.t.*)
    *(.text .rela.text .stub .text.* .gnu.linkonce.t.*)
    *(.rodata .rodata.* .gnu.linkonce.r.*)
    . = ALIGN(4096);
    *(__ipc_entry)
    . = ALIGN(4096);
  }

  /* PROVIDE(etext = .); */	/* Define the 'etext' symbol to this value */
  /*
  .rodata : {
    *(.rodata .rodata.* .gnu.linkonce.r.*)
  }
  */

  /* aligned the data to a (4K) page, so it can be assigned
   different protection than the code*/
  . = ALIGN(4096);

  .data : AT(phys + (data - code)) {
    data = .;
    *(.data .data.*)
    . = ALIGN(4096);
  }

  /* PROVIDE (edata = .); */

  .bss : AT(phys + (bss - code)) {
    bss = .;  
    PROVIDE (__bss_start__ = .);
    *(.bss)
    PROVIDE (__bss_end__ = .);
    . = ALIGN(4096);
  }

  /*PROVIDE (end = .);*/
  end = .;
  . = ALIGN(4096);

  .initial_component : AT(phys + (initial_component - code)) {
     initial_component = .;
     *(.initial_component)
  }

  /DISCARD/ : {
     *(.comment)
     *(.eh_frame)
     *(.note.gnu.build-id)
  }

  /*ASSERT(. < 0x90000000, "Kernel size exceeded the limit, may need to adjust memory configs")*/
}

/* End Of File ***************************************************************/

/* Copyright (C) Evo-Devo Instrum. All rights reserved ***********************/



