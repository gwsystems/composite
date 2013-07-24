#include "printk.h"
#include "multiboot.h"

void
multiboot__print(struct multiboot *mboot)
{
    printk(INFO, "Multiboot\n"
        "\tflags: \n"
        "\tlow mem: %x\n"
        "\thigh mem: %x\n"
        "\tboot_device: %s\n"
        "\tcmdline: %s\n"
	"\tmodules: %d (0x%x)\n"
	,
        mboot->mem_lower,
        mboot->mem_upper,
        (char *)mboot->boot_device,
        (char *)mboot->cmdline,
	mboot->mods_count, mboot->mods_addr

	);
}
