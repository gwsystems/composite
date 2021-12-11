#include "cav7_consts.h"

int
chal_tlb_lockdown(unsigned long entryid, unsigned long vaddr, unsigned long paddr)
{
	/* entryid: 0-3 means lock a 4kB page,
	 * 4-7 means lock a 64kB page,
	 * 8-11 means lock a 1MB page,
	 * 12-15 means lock a 16MB page */
	assert(entryid < 16);
	__cos_cav7_sel_lkdn_write_set(entryid % 4);
	if (entryid < 4) {
		/* Cortex-A9 main TLB VA register */
		__cos_cav7_main_tlb_va_set(vaddr);
		/* Cortex-A9 main TLB attribute register */
		__cos_cav7_main_tlb_attrib_set(0x07);
		/* Cortex-A9 main TLB PA register */
		__cos_cav7_main_tlb_pa_set(paddr | (1 << 6) | (3 << 1) | 1);
	} else if (entryid < 8) {
		__cos_cav7_main_tlb_va_set(vaddr);
		__cos_cav7_main_tlb_attrib_set(0x07);
		__cos_cav7_main_tlb_pa_set(paddr | (2 << 6) | (3 << 1) | 1);
	} else if (entryid < 12) {
		__cos_cav7_main_tlb_va_set(vaddr);
		__cos_cav7_main_tlb_attrib_set(0x07);
		__cos_cav7_main_tlb_pa_set(paddr | (3 << 6) | (3 << 1) | 1);
	} else {
		__cos_cav7_main_tlb_va_set(vaddr);
		__cos_cav7_main_tlb_attrib_set(0x07);
		__cos_cav7_main_tlb_pa_set(paddr | (0 << 6) | (3 << 1) | 1);
	}

	__cos_cav7_ats1cpr_set(vaddr);
	assert((__cos_cav7_par_get() & 0xFFFFF000) == paddr);

	return 0;
}

int
chal_tlbflush(int a)
{
	__cos_cav7_tlbiall_set(a);

	return 0;
}

int
chal_tlbstall(void)
{
	return __cos_cav7_pmxevcntr_get();
}

int
chal_tlbstall_recount(int a)
{
	__cos_cav7_pmxevcntr_set(0);

	return 0;
}
