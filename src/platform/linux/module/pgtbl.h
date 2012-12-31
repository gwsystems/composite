#ifndef PGTBL_H
#define PGTBL_H

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/pgtable.h>
#include "../../../kernel/include/chal.h"

static inline pte_t *
__pgtbl_lookup_address(paddr_t pgtbl, unsigned long addr)
{
	pgd_t *pgd = ((pgd_t *)pa_to_va((void*)pgtbl)) + pgd_index(addr);
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

void zero_pgtbl_range(paddr_t pt, unsigned long lower_addr, unsigned long size);
void copy_pgtbl_range(paddr_t pt_to, paddr_t pt_from, 
		      unsigned long lower_addr, unsigned long size);
void copy_pgtbl(paddr_t pt_to, paddr_t pt_from);
//extern int copy_mm(unsigned long clone_flags, struct task_struct * tsk);
void print_valid_pgtbl_entries(paddr_t pt);

#endif	/* PGTBL_H */
