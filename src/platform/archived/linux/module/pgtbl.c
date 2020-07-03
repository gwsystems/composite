#include "../../../kernel/include/spd.h"
#include "../../../kernel/include/ipc.h"
#include "../../../kernel/include/thread.h"
#include "../../../kernel/include/measurement.h"
#include "../../../kernel/include/mmap.h"

#include "../../../kernel/include/chal.h"
#include "../../../kernel/include/pgtbl.h"
#include "linux_pgtbl.h"

extern struct per_core_cos_thd cos_thd_per_core[NUM_CPU];
int chal_pgtbl_can_switch(void) { return current == cos_thd_per_core[get_cpuid()].cos_thd; }

/* 
 * Namespaces: 
 * pgtbl_*      -> hijack accessible (prototype in pgtbl.h).
 * chal_pgtbl_* -> callable from the composite kernel (chal.h).
 */

int
chal_pgtbl_add(paddr_t pgtbl, vaddr_t vaddr, paddr_t paddr, int flags)
{
	int ret;
	unsigned long kflags = PGTBL_PRESENT | PGTBL_USER | PGTBL_ACCESSED;

	if (flags & MAPPING_RW) kflags |= PGTBL_WRITABLE;

	ret = pgtbl_mapping_add((pgtbl_t)pgtbl, (u32_t)vaddr, (u32_t)paddr, kflags);
	if (ret) return -1;

	return 0;
}

/* 
 * This won't work to find the translation for the argument region as
 * __va doesn't work on module-mapped memory. 
 */
vaddr_t 
chal_pgtbl_vaddr2kaddr(paddr_t pgtbl, unsigned long addr)
{
	u32_t flags;
	vaddr_t kaddr;

	kaddr = pgtbl_translate((pgtbl_t)pgtbl, (u32_t)addr, &flags);

	return kaddr;
}

/*
 * Remove a given virtual mapping from a page table.  Return 0 if
 * there is no present mapping, and the physical address mapped if
 * there is an existant mapping.
 */
paddr_t
chal_pgtbl_rem(paddr_t pgtbl, vaddr_t va)
{
	int ret;
	u32_t flags;
	paddr_t page = pgtbl_lookup((pgtbl_t)pgtbl, va, &flags);
	if (!page) return 0;

	ret = pgtbl_mapping_del_direct((pgtbl_t)pgtbl, (u32_t)va);
	if (ret) return 0;

	return page;
}

void 
pgtbl_print_path(paddr_t pgtbl, unsigned long addr)
{
	u32_t flags;
	pgd_t *pt = pgtbl_get_pgd((pgtbl_t)pgtbl, (u32_t)addr);
	paddr_t pe = pgtbl_lookup((pgtbl_t)pgtbl, (u32_t)addr, &flags);
	
	printk("cos: addr %x, pgd entry - %x, pte entry - %x\n", 
	       (unsigned int)addr, (unsigned int)pgd_val(*pt), (unsigned int)(pe | flags));

	return;
}

/* allocate and link in a page middle directory */
int 
chal_pgtbl_add_middledir(paddr_t pt, unsigned long vaddr)
{
	int ret;
	unsigned long *page = chal_alloc_page(); /* zeroed */
	if (!page) return -1;

	pgtbl_init_pte(page);
	ret = pgtbl_intern_expand((pgtbl_t)pt, (u32_t)vaddr, page, PGTBL_INTERN_DEF);

	return ret;
}

int 
chal_pgtbl_rem_middledir(paddr_t pt, unsigned long vaddr)
{
	/* not tested yet. */
	unsigned long *page;

	page = pgtbl_intern_prune((pgtbl_t)pt, (u32_t)vaddr);
	chal_free_page(page);

	return 0;
}

int 
chal_pgtbl_rem_middledir_range(paddr_t pt, unsigned long vaddr, long size)
{
	unsigned long a;

	for (a = vaddr ; a < vaddr + size ; a += HPAGE_SIZE) {
		BUG_ON(chal_pgtbl_rem_middledir(pt, a));
	}
	return 0;
}

int 
chal_pgtbl_add_middledir_range(paddr_t pt, unsigned long vaddr, long size)
{
	unsigned long a;

	for (a = vaddr ; a < vaddr + size ; a += HPAGE_SIZE) {
		if (chal_pgtbl_add_middledir(pt, a)) {
			chal_pgtbl_rem_middledir_range(pt, vaddr, a-vaddr);
			return -1;
		}
	}
	return 0;
}

unsigned int *
pgtbl_module_to_vaddr(unsigned long addr)
{
	return (unsigned int *)chal_pgtbl_vaddr2kaddr((paddr_t)chal_va2pa(current->mm->pgd), addr);
}

/*
 * Verify that the given address in the page table is present.  Return
 * 0 if present, 1 if not.  *This will check the pgd, not for the pte.*
 */
int 
chal_pgtbl_entry_absent(paddr_t pt, unsigned long addr)
{
	return pgtbl_check_pgd_absent((pgtbl_t)pt, (u32_t)addr);
}

/* Find the nth valid pgd entry */
static unsigned long 
get_valid_pgtbl_entry(paddr_t pt, int n)
{
	int i;

	for (i = 1 ; i < PTRS_PER_PGD ; i++) {
		if (!chal_pgtbl_entry_absent(pt, i*PGDIR_SIZE)) {
			n--;
			if (n == 0) {
				return i*PGDIR_SIZE;
			}
		}
	}
	return 0;
}

void 
pgtbl_print_valid_entries(paddr_t pt) 
{
	int n = 1;
	unsigned long ret;
	printk("cos: valid pgd addresses:\ncos: ");
	while ((ret = get_valid_pgtbl_entry(pt, n++)) != 0) {
		printk("%lx\t", ret);
	}
	printk("\ncos: %d valid addresses.\n", n-1);

	return;
}

void 
chal_pgtbl_zero_range(paddr_t pt, unsigned long lower_addr, unsigned long size)
{
	pgd_t *pgd = pgtbl_get_pgd((pgtbl_t)pt, (u32_t)lower_addr);
	unsigned int span = hpage_index(size);

	if (!(pgd_val(*pgd)) & PGTBL_PRESENT) {
		printk("cos: BUG: nothing to copy from pgd @ %x.\n", 
		       (unsigned int)lower_addr);
	}

	/* sizeof(pgd entry) is intended */
	memset(pgd, 0, span*sizeof(pgd_t));
}

void 
chal_pgtbl_copy_range(paddr_t pt_to, paddr_t pt_from, 
		 unsigned long lower_addr, unsigned long size)
{
	pgd_t *tpgd = pgtbl_get_pgd((pgtbl_t)pt_to, (u32_t)lower_addr);
	pgd_t *fpgd = pgtbl_get_pgd((pgtbl_t)pt_from, (u32_t)lower_addr);
	unsigned int span = hpage_index(size);

	if (!(pgd_val(*fpgd)) & PGTBL_PRESENT) {
		printk("cos: BUG: nothing to copy from pgd @ %x.\n", 
		       (unsigned int)lower_addr);
	}

	/* sizeof(pgd entry) is intended */
	memcpy(tpgd, fpgd, span*sizeof(pgd_t));
}

void 
chal_pgtbl_copy_range_nocheck(paddr_t pt_to, paddr_t pt_from, 
			 unsigned long lower_addr, unsigned long size)
{
	pgd_t *tpgd = pgtbl_get_pgd((pgtbl_t)pt_to, (u32_t)lower_addr);
	pgd_t *fpgd = pgtbl_get_pgd((pgtbl_t)pt_from, (u32_t)lower_addr);
	unsigned int span = hpage_index(size);

	/* sizeof(pgd entry) is intended */
	memcpy(tpgd, fpgd, span*sizeof(pgd_t));
}

/* Copy pages non-empty in from, and empty in to */
void 
copy_pgtbl_range_nonzero(paddr_t pt_to, paddr_t pt_from, 
			 unsigned long lower_addr, unsigned long size)
{
	pgd_t *tpgd = pgtbl_get_pgd((pgtbl_t)pt_to, (u32_t)lower_addr);
	pgd_t *fpgd = pgtbl_get_pgd((pgtbl_t)pt_from, (u32_t)lower_addr);
	unsigned int span = hpage_index(size);
	int i;

	printk("Copying from %p:%d to %p.\n", fpgd, span, tpgd);

	/* sizeof(pgd entry) is intended */
	for (i = 0 ; i < span ; i++) {
		if (!(pgd_val(tpgd[i]) & PGTBL_PRESENT)) {
			if (pgd_val(fpgd[i]) & PGTBL_PRESENT) printk("\tcopying vaddr %lx.\n", lower_addr + i * HPAGE_SHIFT);
			memcpy(&tpgd[i], &fpgd[i], sizeof(pgd_t));
		}
	}
}

void 
pgtbl_copy(paddr_t pt_to, paddr_t pt_from)
{
	chal_pgtbl_copy_range_nocheck(pt_to, pt_from, 0, 0xFFFFFFFF);
}
