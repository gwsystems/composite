#include "assert.h"
#include "kernel.h"
#include "multiboot.h"
#include "string.h"
#include "boot_comp.h"
#include "mem_layout.h"
#include "chal_cpu.h"
#include "board_specifics.h"

#include <captbl.h>
#include <retype_tbl.h>
#include <component.h>
#include <thd.h>

#define ADDR_STR_LEN 8
#define CMDLINE_MAX_LEN 32
#define CMDLINE_REQ_LEN (ADDR_STR_LEN * 2 + 1)

struct mem_layout glb_memlayout;
volatile int      cores_ready[NUM_CPU];

static int
xdtoi(char c)
{
	if ('0' <= c && c <= '9') return c - '0';
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	return 0;
}

static u32_t
hextol(const char *s)
{
	int i, r = 0;

	for (i = 0; i < 8; i++) { r = (r * 0x10) + xdtoi(s[i]); }

	return r;
}

extern u8_t end, end_all; /* from the linker script */
extern u8_t _binary_constructor_start, _binary_constructor_end;

#define MEM_KB_ONLY(x) (((x) & ((1 << 20) - 1)) >> 10)
#define MEM_MB_ONLY(x) ((x) >> 20)

void
kern_memory_setup(struct multiboot *mb, u32_t mboot_magic)
{
	/* Empty: no mboot information used here */
}

void
kmain(struct multiboot *mboot, u32_t mboot_magic, u32_t esp)
{
	serial_init();

	printk("serial initialization complete\r\n");

	kern_paging_map_init(0);

	chal_init();
	cap_init();
	ltbl_init();
	retype_tbl_init();
	comp_init();
	thd_init();

	/* We know the memory layout and will initialize it here - kernel : 256MB */
	glb_memlayout.kern_end = &end_all;
	/* Booter : 64MB, these are kernel addresses that will never be used in fact */
	glb_memlayout.mod_start = &_binary_constructor_start;
	glb_memlayout.mod_end   = &_binary_constructor_end;
	printk("[%08x, %08x)\n", &_binary_constructor_start, &_binary_constructor_end);
	/* Booter entry */
	glb_memlayout.bootc_entry = 0; /* use elf library for this later! */
	/* Booter virtual address */
	glb_memlayout.bootc_vaddr    = 0;
	glb_memlayout.kern_boot_heap = mem_boot_start(); /* this breaks if bss segment is after the initial component! */
	glb_memlayout.kmem_end       = KERN_MEM_END;
	printk("[%08x, %08x)\n", glb_memlayout.kern_boot_heap, glb_memlayout.kmem_end);
	glb_memlayout.allocs_avail   = 1;

	chal_kernel_mem_pa = chal_va2pa(mem_kmem_start());

	paging_init();

	kern_boot_comp(INIT_CORE);
	kern_boot_upcall();

	/* should not get here... */
	khalt();
}

void
smp_kmain(void)
{
	volatile cpuid_t           cpu_id   = get_cpuid();
	struct cos_cpu_local_info *cos_info = cos_cpu_local_info();

	printk("Initializing CPU %d\n", cpu_id);

	chal_cpu_init();
	kern_boot_comp(cpu_id);

	printk("New CPU %d Booted\n", cpu_id);
	cores_ready[cpu_id] = 1;
	/* waiting for all cored booted */
	while (cores_ready[INIT_CORE] == 0)
		;

	kern_boot_upcall();

	while (1)
		;
}

void
khalt(void)
{
	while (1)
		;
}
