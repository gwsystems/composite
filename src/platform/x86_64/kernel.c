#include <types.h>
#include <consts.h>
#include <cos_error.h>
#include <assert.h>
#include <kernel.h>
#include <multiboot2.h>
#include <string.h>
#include <chal_cpu.h>

#include <resources.h>
#include <state.h>
#include <fpu.h>
#include <init.h>
#include <cos_elf_loader.h>

/* Track boot progress across all cores. */
volatile int cores_ready[COS_NUM_CPU];

#define MEM_KB_ONLY(x) (((x) & ((1 << 20) - 1)) >> 10)
#define MEM_MB_ONLY(x) ((x) >> 20)

extern u8_t _binary_constructor_start[], _binary_constructor_end[];

static void
multiboot_mem_parse(struct multiboot_tag *tag)
{
	unsigned int i = 0;
	multiboot_memory_map_t *mmap;
	u8_t *                  mem_addr;
	unsigned long long      mem_len;

	for (mmap = ((struct multiboot_tag_mmap *)tag)->entries;
	     (multiboot_uint8_t *)mmap < (multiboot_uint8_t *)tag + tag->size;
	     mmap = (multiboot_memory_map_t *)((unsigned long)mmap + ((struct multiboot_tag_mmap *)tag)->entry_size)) {

		mem_addr = chal_pa2va((paddr_t)mmap->addr);
		mem_len  = mmap->len; /* maximum allowed */
		printk("\t- %d (%s): [%08llx, %08llx) sz = %ldMB + %ldKB\n", i,
			mmap->type == 1 ? "Available" : "Reserved ",
			mmap->addr, mmap->addr + mmap->len,
			MEM_MB_ONLY((unsigned long long)mmap->len), MEM_KB_ONLY((unsigned long long)mmap->len));

		i++;
	}
}

static void
multiboot_tag_parse(unsigned long mboot_addr)
{
	struct multiboot_tag *tag;

	for (tag = (struct multiboot_tag *) (mboot_addr + 8);
		tag->type != MULTIBOOT_TAG_TYPE_END;
		tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
		if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) multiboot_mem_parse(tag);
	}
}

void
multiboot_output(unsigned long mboot_addr, unsigned long mboot_magic)
{
	u32_t size;

	if (mboot_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
		die("multiboot magic not correct\n");
	}

	if (mboot_addr & 7) {
		die("mboot unligned mbi\n");
	}

	printk("Untyped pages:\n");
	printk("\t- [%p, %p)\n", &pages[0], &pages[COS_NUM_RETYPEABLE_PAGES - 1]);
	printk("Initial component found:\n");
	printk("\t- [%p, %p)\n", &_binary_constructor_start, &_binary_constructor_end);

	size = *(u32_t *)mboot_addr;
	if (size <= 0) {
		die("Can't properly find multiboot tag!\n");
	}

	printk("Memory regions:\n");
	multiboot_tag_parse(mboot_addr);
}

volatile vaddr_t entry_address;

void
kmain(unsigned long mboot_addr, unsigned long mboot_magic)
{
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))
	int r;
	cos_retval_t rv;
	uword_t max;
	uword_t post_constructor;
	struct elf_hdr *h;

	vaddr_t ro_addr, rw_addr;
	size_t ro_sz = 0, data_sz = 0, bss_sz = 0;
	char *ro_src = NULL, *data_src = NULL;

	tss_init(INIT_CORE);
	gdt_init(INIT_CORE);
	idt_init(INIT_CORE);
	serial_init();

	max = MAX((unsigned long)chal_va2pa((void*)mboot_addr), (unsigned long)(chal_va2pa(&end)));

	kern_paging_map_init((void *)(max));
	multiboot_output(mboot_addr, mboot_magic);
	chal_state_init();

	chal_init();
	paging_init();
	acpi_init();
	lapic_init();
	timer_init();

	post_constructor = ((uword_t)&_binary_constructor_end - (uword_t)pages) / COS_PAGE_SIZE;
	kernel_init(post_constructor);

	h = (void *)&_binary_constructor_start;
	r = elf_load_info(h, &ro_addr, &ro_sz, &ro_src, &rw_addr, &data_sz, &data_src, &bss_sz);
	assert(r == 0);
	/* Linker script should have placed the elf object at offset 1 in the page array */
	assert(((uword_t)&_binary_constructor_start - (uword_t)pages) == COS_PAGE_SIZE);
	rv = constructor_init(post_constructor,
			      ro_addr, elf_entry_addr(h),
			      (uword_t)ro_src - (uword_t)h, ro_sz,
			      (uword_t)data_src - (uword_t)h, data_sz,
			      bss_sz);
	assert(rv == COS_RET_SUCCESS);
	kernel_core_init(INIT_CORE);

	smp_init(cores_ready);
	cores_ready[INIT_CORE] = 1;
	entry_address = elf_entry_addr(h);

	constructor_core_execute(0, entry_address);

	/* should not get here... */
	khalt();
}

void
smp_kmain(void)
{
	volatile coreid_t cpu_id = coreid();

	printk("Initializing CPU %d\n", cpu_id);
	tss_init(cpu_id);
	gdt_init(cpu_id);
	idt_init(cpu_id);

	chal_cpu_init();
	lapic_init();

	printk("New CPU %d Booted\n", cpu_id);
	cores_ready[cpu_id] = 1;
	/* waiting for all cores to boot */
	while(cores_ready[INIT_CORE] == 0);

	constructor_core_execute(cpu_id, entry_address);

	while(1) ;
}

extern void shutdown_apm(void);
extern void outw(unsigned short __val, unsigned short __port);

__attribute__((noreturn)) void
khalt(void)
{
	static int method = 0;

	if (method == 0) printk("Shutting down...\n");

	/*
	 * Use the case statement as we shutdown in the fault handler,
	 * thus faults on shutdown require that we bypass faulty
	 * shutdown handlers.
	 */
	switch(method) {
	case 0:
		method++;
		printk("\ttry acpi shutdown...\n");
		acpi_shutdown();
		printk("...FAILED\n");
	case 1:
		method++;
		printk("\ttry apm shutdown...\n");
		shutdown_apm();
		printk("...FAILED\n");
	case 2:
		method++;
		printk("\t...try emulator magic shutdown...\n");
		outw(0x0 | 0x2000, 0xB004);
		printk("...FAILED\n");
	}
	/* last resort */
	printk("\t...spinning\n");
	while (1) ;
}
