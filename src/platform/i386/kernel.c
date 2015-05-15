#define ENABLE_SERIAL
#define ENABLE_TIMER

#include "assert.h"
#include "kernel.h"
#include "multiboot.h"
#include "string.h"
#include "comp.h"
#include "mem_layout.h"

#include <captbl.h>
#include <retype_tbl.h>
#include <component.h>
#include <thd.h>
#include <inv.h>
#include <mmap.h>

void
khalt(void)
{
	printk("Shutting down...\n");
	asm("mov $0x53,%ah");
	asm("mov $0x07,%al");
	asm("mov $0x001,%bx");
	asm("mov $0x03,%cx");
	asm("int $0x15");
}

struct mem_layout glb_memlayout;

void
memory_validate(struct multiboot *mb, u32_t mboot_magic)
{
	struct multiboot_mod_list *mods;
	struct multiboot_mem_list *mems;
	unsigned int i, wastage = 0;

	if (mboot_magic != MULTIBOOT_EAX_MAGIC) {
		die("Not started from a multiboot loader!\n");
	}
	if ((mb->flags & MULTIBOOT_FLAGS_REQUIRED) != MULTIBOOT_FLAGS_REQUIRED) {
		die("Multiboot flags include %x but are missing one of %x\n", 
		    mb->flags, MULTIBOOT_FLAGS_REQUIRED);
	}

	mods = (struct multiboot_mod_list *)mb->mods_addr;
	mems = (struct multiboot_mem_list *)mb->mmap_addr;

	if (mb->mods_count != 1) {
		die("Boot failure: expecting a single module to load, received %d instead.\n", mb->mods_count);
	}

	printk("System info (from multiboot):\n");
	printk("\tModules:\n");
	for (i = 0 ; i < mb->mods_count ; i++) {
		struct multiboot_mod_list *mod = &mods[i];
		
		printk("\t- %d: [%08x, %08x)\n", i, mod->mod_start, mod->mod_end);
		/* These values have to be higher-half addresses */
		glb_memlayout.mod_start = chal_pa2va((paddr_t)mod->mod_start);
		glb_memlayout.mod_end   = chal_pa2va((paddr_t)mod->mod_end);
	}
	glb_memlayout.kern_boot_heap = mem_boot_start();
	printk("\tMemory regions:\n");
	for (i = 0 ; i < mb->mmap_length/sizeof(struct multiboot_mem_list) ; i++) {
		struct multiboot_mem_list *mem = &mems[i];
		
		printk("\t- %d (%s): [%08llx, %08llx)\n", i, 
		       mem->type == 1 ? "Available" : "Reserved ", mem->addr, mem->addr + mem->len);
	}
	/* FIXME: check memory layout vs. the multiboot memory regions... */

	/* Validate the memory layout. */
	assert(mem_kern_end() <= mem_bootc_start());
	assert(mem_bootc_end() <= mem_boot_start());
	assert(mem_boot_start() <= mem_kmem_start());
	assert(mem_kmem_end() <= mem_usermem_start());

	wastage += mem_boot_start() - mem_bootc_end();
	wastage += mem_usermem_start() - mem_kmem_end();
	printk("Amount of wasted memory due to layout is %x\n", wastage);
}

void 
kmain(struct multiboot *mboot, u32_t mboot_magic, u32_t esp)
{
	int bc = 0, ret;

	tss_init();
	gdt_init();
	idt_init();

#ifdef ENABLE_SERIAL
	serial_init();
#endif

#ifdef ENABLE_CONSOLE
	console_init();
#endif

#ifdef ENABLE_TIMER
	timer_init(100);
#endif	    
	memory_validate(mboot, mboot_magic);

	cos_init_memory();
	cap_init();
       	ltbl_init();
       	retype_tbl_init();
       	comp_init();
       	thd_init();
       	inv_init();
	paging_init();

	ret = kern_boot_comp();
	assert(ret == 0);

	die("to the next step!!!\n");
	user_init();
	khalt(); 
}
