OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

PROVIDE(__va_offset__ = 0x80000000);
phys = 0x00100000;
SECTIONS
{
  . = 0x80100000;

  .text : AT(phys) {
    code = .;
    *start.o(.text) /* .rela.text .stub .text.* .gnu.linkonce.t.*) */
    *(.text) /* .rela.text .stub .text.* .gnu.linkonce.t.*) */
    *(.rodata) /*.rodata.* .gnu.linkonce.r.*) */
    . = ALIGN(4096);
    *(__ipc_entry)
    . = ALIGN(4096);
  }

  .data : AT(phys + (data - code)) {
    data = .;
    *(.data .data.*)
    . = ALIGN(4096);
  }

  end = .;
  . = ALIGN(4096);

  .initial_component : AT(phys + (initial_component - code)) {
     initial_component = .;
     *(.initial_component)
     . = ALIGN(4096);
  }

  .bss : AT(phys + (bss - code)) {
    bss = .;  
    __bss_start__ = .;
    *(.bss)
    __bss_end__ = .;
    . = ALIGN(4096);
  }

  end_all = .;
  ASSERT(. < 0x90000000, "Kernel size exceeded the limit, may need to adjust memory configurations")


  /DISCARD/ : {
     *(.comment)
     *(.eh_frame)
     *(.note.gnu.build-id)
  }
}
