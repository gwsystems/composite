#include "../../../kernel/include/spd.h"
#include "../../../kernel/include/ipc.h"
#include "../../../kernel/include/thread.h"
#include "../../../kernel/include/measurement.h"
#include "../../../kernel/include/mmap.h"

#include "../../../kernel/include/chal.h"
#include "pgtbl.h"

extern struct per_core_cos_thd cos_thd_per_core[NUM_CPU];
int chal_pgtbl_can_switch(void) { return current == cos_thd_per_core[get_cpuid()].cos_thd; }


static pte_t *
pgtbl_lookup_address(paddr_t pgtbl, unsigned long addr)
{
	pgd_t *pgd = ((pgd_t *)chal_pa2va((void*)pgtbl)) + pgd_index(addr);
	pud_t *pud;
	pmd_t *pmd;
	if (pgd_none(*pgd)) {
		return NULL;
	}
	pud = pud_offset(pgd, addr);
	if (pud_none(*pud)) {
		return NULL;
	}
	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd)) {
		return NULL;
	}
	if (pmd_large(*pmd))
		return (pte_t *)pmd;
        return pte_offset_kernel(pmd, addr);
}

/* 
 * Namespaces: 
 * pgtbl_*      -> hijack accessible (prototype in pgtbl.h).
 * chal_pgtbl_* -> callable from the composite kernel (chal.h).
 */

int
chal_pgtbl_add(paddr_t pgtbl, vaddr_t vaddr, paddr_t paddr, int flags)
{
	pte_t *pte = pgtbl_lookup_address(pgtbl, (unsigned long)vaddr);
	unsigned long kflags = _PAGE_PRESENT | _PAGE_USER | _PAGE_ACCESSED;

	if (flags & MAPPING_RW) kflags |= _PAGE_RW;
	if (!pte || pte_val(*pte) & _PAGE_PRESENT) return -1;
	pte->pte_low = ((unsigned long)paddr) | kflags;

	return 0;
}

/* 
 * This won't work to find the translation for the argument region as
 * __va doesn't work on module-mapped memory. 
 */
vaddr_t 
chal_pgtbl_vaddr2kaddr(paddr_t pgtbl, unsigned long addr)
{
	pte_t *pte = pgtbl_lookup_address(pgtbl, addr);
	unsigned long kaddr;

	if (!pte || !(pte_val(*pte) & _PAGE_PRESENT)) {
		return 0;
	}
	
	/*
	 * 1) get the value in the pte
	 * 2) map out the non-address values to get the physical address
	 * 3) convert the physical address to the vaddr
	 * 4) offset into that vaddr the appropriate amount from the addr arg.
	 * 5) return value
	 */

	kaddr = (unsigned long)__va(pte_val(*pte) & PAGE_MASK) + (~PAGE_MASK & addr);

	return (vaddr_t)kaddr;
}

/*
 * Remove a given virtual mapping from a page table.  Return 0 if
 * there is no present mapping, and the physical address mapped if
 * there is an existant mapping.
 */
paddr_t
chal_pgtbl_rem(paddr_t pgtbl, vaddr_t va)
{
	pte_t *pte = pgtbl_lookup_address(pgtbl, va);
	paddr_t val;

	if (!pte || !(pte_val(*pte) & _PAGE_PRESENT)) {
		return 0;
	}
	val = (paddr_t)(pte_val(*pte) & PAGE_MASK);
	pte->pte_low = 0;

	return val;
}

void 
pgtbl_print_path(paddr_t pgtbl, unsigned long addr)
{
	pgd_t *pt = ((pgd_t *)chal_pa2va((void*)pgtbl)) + pgd_index(addr);
	pte_t *pe = pgtbl_lookup_address(pgtbl, addr);
	
	printk("cos: addr %x, pgd entry - %x, pte entry - %x\n", 
	       (unsigned int)addr, (unsigned int)pgd_val(*pt), (unsigned int)pte_val(*pe));

	return;
}

/* allocate and link in a page middle directory */
int 
chal_pgtbl_add_middledir(paddr_t pt, unsigned long vaddr)
{
	pgd_t *pgd = ((pgd_t *)chal_pa2va((void*)pt)) + pgd_index(vaddr);
	unsigned long *page;

	page = chal_alloc_page(); /* zeroed */
	if (!page) return -1;

	pgd->pgd = (unsigned long)chal_va2pa(page) | _PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED;
	return 0;
}

int 
chal_pgtbl_rem_middledir(paddr_t pt, unsigned long vaddr)
{
	pgd_t *pgd = ((pgd_t *)chal_pa2va((void*)pt)) + pgd_index(vaddr);
	unsigned long *page;

	page = (unsigned long *)chal_pa2va((void*)(pgd->pgd & PTE_PFN_MASK));
	pgd->pgd = 0;
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
	pgd_t *pgd = ((pgd_t *)chal_pa2va((void*)pt)) + pgd_index(addr);

	return !((pgd_val(*pgd)) & _PAGE_PRESENT);
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
	pgd_t *pgd = ((pgd_t *)chal_pa2va((void*)pt)) + pgd_index(lower_addr);
	unsigned int span = hpage_index(size);

	if (!(pgd_val(*pgd)) & _PAGE_PRESENT) {
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
	pgd_t *tpgd = ((pgd_t *)chal_pa2va((void*)pt_to)) + pgd_index(lower_addr);
	pgd_t *fpgd = ((pgd_t *)chal_pa2va((void*)pt_from)) + pgd_index(lower_addr);
	unsigned int span = hpage_index(size);

	if (!(pgd_val(*fpgd)) & _PAGE_PRESENT) {
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
	pgd_t *tpgd = ((pgd_t *)chal_pa2va((void*)pt_to)) + pgd_index(lower_addr);
	pgd_t *fpgd = ((pgd_t *)chal_pa2va((void*)pt_from)) + pgd_index(lower_addr);
	unsigned int span = hpage_index(size);

	/* sizeof(pgd entry) is intended */
	memcpy(tpgd, fpgd, span*sizeof(pgd_t));
}

/* Copy pages non-empty in from, and empty in to */
void 
copy_pgtbl_range_nonzero(paddr_t pt_to, paddr_t pt_from, 
			 unsigned long lower_addr, unsigned long size)
{
	pgd_t *tpgd = ((pgd_t *)chal_pa2va((void*)pt_to)) + pgd_index(lower_addr);
	pgd_t *fpgd = ((pgd_t *)chal_pa2va((void*)pt_from)) + pgd_index(lower_addr);
	unsigned int span = hpage_index(size);
	int i;

	printk("Copying from %p:%d to %p.\n", fpgd, span, tpgd);

	/* sizeof(pgd entry) is intended */
	for (i = 0 ; i < span ; i++) {
		if (!(pgd_val(tpgd[i]) & _PAGE_PRESENT)) {
			if (pgd_val(fpgd[i]) & _PAGE_PRESENT) printk("\tcopying vaddr %lx.\n", lower_addr + i * HPAGE_SHIFT);
			memcpy(&tpgd[i], &fpgd[i], sizeof(pgd_t));
		}
	}
}

void 
pgtbl_copy(paddr_t pt_to, paddr_t pt_from)
{
	chal_pgtbl_copy_range_nocheck(pt_to, pt_from, 0, 0xFFFFFFFF);
}
