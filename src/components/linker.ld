OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

PROVIDE(__va_offset__ = 0x00000000);

SECTIONS
{
  /* the entry point, before enabling paging. The code to enable paing
   needs to have the same virtual/physical address. entry.S and start.c
   run in this initial setting.*/
  . = 0x10000000;

  .text : AT(__va_offset__) {
    *inv.o(.text .rela.text .stub .text.* .gnu.linkonce.t.*)
    *(.text .rela.text .stub .text.* .gnu.linkonce.t.*)
  }

  PROVIDE(etext = .);	/* Define the 'etext' symbol to this value */

  .rodata : {
    *(.rodata .rodata.* .gnu.linkonce.r.*)
  }

  /* aligned the data to a (4K) page, so it can be assigned
   different protection than the code*/
  . = ALIGN(0x1000);

  .data : {
    *(.data .data.*)
  }

  PROVIDE (edata = .);

  .bss : {  
    PROVIDE (__bss_start__ = .);
    *(.bss .bss.* COMMON)
    PROVIDE (__bss_end__ = .);
  }

  . = ALIGN(0x1000);
  PROVIDE (end = .);
  ASSERT(. < 0x14000000, "User size exceeded the limit, may need to adjust memory configs")
}

/* End Of File ***************************************************************/

/* Copyright (C) Evo-Devo Instrum. All rights reserved ***********************/



