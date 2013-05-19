#include "printk.h"
#include "multiboot.h"

void
multiboot__print(struct multiboot *mboot)
{
    printk(INFO, "Multiboot\n"
        "flags: \n",
        "low mem: %lu\n",
        "high mem: %lu\n",
        "boot_device: %s\n",
        "cmdline: %s\n",
        mboot->mem_lower,
        mboot->mem_upper,
        (char *)mboot->boot_device,
        (char *)mboot->cmdline);
}
