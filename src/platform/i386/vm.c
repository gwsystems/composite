#include <pgtbl.h>

#include "kernel.h"
#include "string.h"
#include "isr.h"
#include "chal_cpu.h"
#include "mem_layout.h"

#define POSSIBLE_FRAMES 1024*1024
#define USER_STACK_SIZE PAGE_SIZE

u32_t user_entry_point;
u32_t user_stack_address;

struct tlb_quiescence tlb_quiescence[NUM_CPU] CACHE_ALIGNED;
struct liveness_entry __liveness_tbl[LTBL_ENTS];
#define KERN_INIT_PGD_IDX (COS_MEM_KERN_START_VA>>PGD_SHIFT)
u32_t boot_comp_pgd[PAGE_SIZE/sizeof(u32_t)] PAGE_ALIGNED = {
	[0]                 = 0 | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER,
	[KERN_INIT_PGD_IDX] = 0 | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER
};
pgtbl_t pgtbl;

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

	for (i = 0 ; i < 8 ; i++) {
		r = (r * 0x10) + xdtoi(s[i]);
	}

	return r;
}

static void
page_fault(struct registers *regs)
{
	u32_t fault_addr, errcode = 0, eip = 0;
    
	fault_addr = chal_cpu_fault_vaddr(regs);
	errcode    = chal_cpu_fault_errcode(regs);

	die("Page Fault (%s%s%s%s%s) at 0x%x, eip 0x%x\n",
	    errcode & PGTBL_PRESENT ? "" : "not-present ",
	    errcode & PGTBL_WRITABLE ? "read-only " : "read-fault ",
	    errcode & PGTBL_USER ? "user-mode " : "system ",
	    errcode & PGTBL_WT ? "reserved " : "",
	    errcode & PGTBL_NOCACHE ? "instruction-fetch " : "",
	    fault_addr, eip);
}

int
kern_setup_image(void)
{
	unsigned long i, j, ptr;
	void *k;
	paddr_t kern_pa_start, kern_pa_end;

	printk("\tSetting up initial page directory.\n");
	pgtbl         = (pgtbl_t)chal_va2pa(boot_comp_pgd);
	kern_pa_start = round_to_pgd_page(chal_va2pa(mem_kern_start())); /* likely 0 */
	kern_pa_end   = chal_va2pa(mem_kmem_end());

	/* FIXME: should use pgtbl_extend instead of directly accessing the pgd array... */
	for (i = kern_pa_start, j = COS_MEM_KERN_START_VA/PGD_RANGE ; 
	     i < (unsigned long)round_up_to_pgd_page(kern_pa_end) ;
	     i += PGD_RANGE, j++) {
		assert(j != KERN_INIT_PGD_IDX || (boot_comp_pgd[j] | PGTBL_GLOBAL) == 
		       (i | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER | PGTBL_GLOBAL));
		boot_comp_pgd[j] = i | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER | PGTBL_GLOBAL;
		//printk("\t\tpgd @ %p -> physical address %p.\n", j*PGD_RANGE, i);
	}
	boot_comp_pgd[0] = 0; 	/* no need for the identity mapping anymore */
	for (k = mem_kern_start() ; k < mem_kern_end() ; k += PAGE_SIZE * RETYPE_MEM_NPAGES) {
		if ((ptr = retypetbl_retype2kern((void*)(chal_va2pa(k)))) != 0) {
			die("retypetbl_retype2kern(%08x) returned %d\n", k, ptr);
		}
	}

	chal_cpu_init();
	chal_cpu_paging_activate(pgtbl); 
	printk("\tInitial page directory initialized.\n");

	return 0;
}

void
paging_init(void)
{
	char *cmdline;
	u32_t cr0, i, j, user_stack_physical = 0;
	int ptr = 0, ret;
	unsigned int spages = USER_STACK_SIZE / PAGE_SIZE;
	u32_t module_address = 0;
	unsigned int mpages;

	printk("Initializing virtual memory\n");
	register_interrupt_handler(14, page_fault);
	if ((ret = kern_setup_image())) {
		die("Could not set up kernel image, errno %d.\n", ret);
	}

#ifdef NIL

	for (i = 0; i < 1024; i++) {
		if ((ptr = pgtbl_intern_expand(pgtbl, i * PAGE_SIZE * 1024, &pte[i], PGTBL_WRITABLE | PGTBL_PRESENT | PGTBL_GLOBAL)) != 0) {
			die("pgtbl_intern_expand() returned %d expanding entry %d\n", ptr, i);
		}
	}

	/* Identity map the kernel */
	printk("Identity mapping kernel from 0x%08x to 0x%08x\n", COS_MEM_KERN_PA, (u32_t)mod);
	for (i = COS_MEM_KERN_PA ; i < (u32_t)mod ; i += PAGE_SIZE * RETYPE_MEM_NPAGES) {
		if ((ptr = retypetbl_retype2kern((void*)(i))) != 0) {
			die("retypetbl_retype2kern(%08x) returned %d\n", i, ptr);
		}
	}
	for (i = COS_MEM_KERN_PA ; i < (u32_t)mod ; i += PAGE_SIZE) {
		if ((ptr = pgtbl_mapping_add(pgtbl, i, i, PGTBL_WRITABLE | PGTBL_PRESENT | PGTBL_GLOBAL)) != 0) {
			die("pgtbl_mapping_add() returned %d mapping kernel page at 0x%x\n", ptr, i);
		}
	}

	/* Map user modules into userspace */
	mpages = ((mod->mod_end - mod->mod_start) / (PAGE_SIZE)) + 1;
	cmdline = (char*)mod->cmdline;
	module_address = hextol(cmdline);
	printk("Mapping Multiboot Module \"%s\" [physical address range %x:%x] @ 0x%08x (%d pages)\n",
	       mod->cmdline, mod->mod_start, mod->mod_end, module_address, mpages);
	
	if (cmdline[8] == '-') {
		user_entry_point = hextol(&cmdline[9]);
	}
	
	for (i = 0 ; i <= mpages ; i += RETYPE_MEM_NPAGES) {
		if ((ptr = retypetbl_retype2user((void*)(mod->mod_start + (i * PAGE_SIZE)))) != 0) {
			die("retypetbl_retype2user(%08x) returned %d\n", mod->mod_start + (i * PAGE_SIZE), ptr);
		}
	}
	for (i = 0 ; i <= mpages ; i++) {
		if ((ptr = pgtbl_mapping_add(pgtbl, module_address + (i * PAGE_SIZE), mod->mod_start + (i * PAGE_SIZE), PGTBL_WRITABLE | PGTBL_PRESENT | PGTBL_USER)) != 0) {
			die("pgtbl_mapping_add() returned %d mapping page %d of module %d\n", ptr, i, i);
		}
	}
	if (mod->mod_end > user_stack_physical) {
		user_stack_physical = mod->mod_end;
	}
	user_stack_physical = (user_stack_physical + USER_STACK_SIZE + PAGE_SIZE) & PGTBL_FRAME_MASK;
	user_stack_address = 0x7fff0000;

	printk("Reserving a user-space stack at v:0x%08x, p:0x%08x\n", user_stack_address, user_stack_physical);
	for (i = 0 ; i < spages ; i++) {
		if ((ptr = retypetbl_retype2user((void*)user_stack_physical + (i * PAGE_SIZE * RETYPE_MEM_NPAGES))) != 0)
			printk("retypetbl_retype2user(%08x) returned %d\n", i, ptr);
	}
	for (i = 0 ; i < spages ; i++) {
		if ((ptr = pgtbl_mapping_add(pgtbl, user_stack_address + (i * PAGE_SIZE), user_stack_physical + (i * PAGE_SIZE), PGTBL_WRITABLE | PGTBL_PRESENT | PGTBL_USER)) != 0) {
			die("pgtbl_mapping_add() returned %d mapping page %d of user stack\n", ptr, i);
		}
	}
#endif
/*
	printk("Enabling paging\n");
	pgtbl_update(pgtbl);
	printk("Switching cr0\n");
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	cr0 |= 0x80000000;
	asm volatile("mov %0, %%cr0" : : "r"(cr0));
	printk("Switched\n");
*/
}
