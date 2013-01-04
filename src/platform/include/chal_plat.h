#ifndef CHAL_PLAT_H
#define CHAL_PLAT_H

#include <linux/sched.h>
#include <linux/kernel.h>

void *chal_pa2va(void *pa);

struct per_core_cos_thd
{
	struct task_struct *cos_thd;
} CACHE_ALIGNED;

extern struct per_core_cos_thd cos_thd_per_core[NUM_CPU];

static inline void 
__chal_pgtbl_switch(paddr_t pt)
{
	struct mm_struct *mm;
	struct task_struct *cos_thd;

	cos_thd = cos_thd_per_core[get_cpuid()].cos_thd;

	BUG_ON(!cos_thd);
	/* 
	 * We aren't doing reference counting here on the mm (via
	 * get_task_mm) because we know that this mm will survive
	 * until the module is unloaded (i.e. it is refcnted at a
	 * granularity of the creation of the composite file
	 * descriptor open/close.)
	 */
	mm = cos_thd->mm;
	mm->pgd = (pgd_t *)chal_pa2va((void*)pt);

	return;
}

/*
 * If for some reason Linux preempts the composite thread, then when
 * it starts it back up again, it needs to know what page tables to
 * use.  Thus update the current mm_struct.
 */
static inline void 
chal_pgtbl_switch(paddr_t pt)
{
	native_write_cr3(pt);
	__chal_pgtbl_switch(pt);
}

static inline unsigned int 
hpage_index(unsigned long n)
{
        unsigned int idx = n >> HPAGE_SHIFT;
        return (idx << HPAGE_SHIFT) != n ? idx + 1 : idx;
}

#endif	/* CHAL_PLAT_H */
