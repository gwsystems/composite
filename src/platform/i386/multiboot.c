#include "printk.h"
#include "multiboot.h"

u32_t
multiboot__print(struct multiboot *mboot)
{
    u32_t top = 0;

    printk(INFO, "Multiboot\n"
        "\tflags: %x\n"
        "\tlow mem: %x\n"
        "\thigh mem: %x\n"
        "\tboot_device: %s\n"
        "\tcmdline: %s\n"
	"\tmodules: %d (0x%x)\n"
	,
        mboot->flags,
        mboot->mem_lower,
        mboot->mem_upper,
        (char *)mboot->boot_device,
        (char *)mboot->cmdline,
	mboot->mods_count, mboot->mods_addr

	);

    if (mboot->mods_count > 0) {
      unsigned int i = 0;
      multiboot_module_t *mod = (multiboot_module_t*)mboot->mods_addr;
      for (i = 0; i < mboot->mods_count; i++) {
	printk(INFO, "Multiboot Module %d \"%s\" [%x:%x]\n", i, mod[i].cmdline, mod[i].mod_start, mod[i].mod_end);
      top = mod[i].mod_end;
      }
    }
    return top;
}
