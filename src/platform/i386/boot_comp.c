#include "assert.h"
#include "kernel.h"
#include "boot_comp.h"
#include "chal_cpu.h"
#include "mem_layout.h"
#include "string.h"
#include <pgtbl.h>
#include <thd.h>
#include <component.h>
#include <inv.h>
#include <hw.h>

extern u8_t *boot_comp_pgd;

int boot_nptes(unsigned int sz) { return round_up_to_pow2(sz, PGD_RANGE)/PGD_RANGE; }

int
boot_pgtbl_mappings_add(struct captbl *ct, pgtbl_t pgtbl, capid_t ptecap, const char *label,
			void *kern_vaddr, unsigned long user_vaddr, unsigned int range, int uvm)
{
	int ret;
	u8_t *ptes;
	unsigned int nptes = 0, i;
	struct cap_pgtbl *pte_cap;

	nptes = boot_nptes(range);
	ptes = mem_boot_alloc(nptes);
	assert(ptes);
	printk("\tCreating %d %s PTEs for PGD @ 0x%x from [%x,%x) to [%x,%x).\n",
	       nptes, label, chal_pa2va((paddr_t)pgtbl),
	       kern_vaddr, kern_vaddr+range, user_vaddr, user_vaddr+range);

	/*
	 * Note the use of NULL here.  We aren't actually adding a PTE
	 * currently.  This is a hack and only used on boot-up.  We'll
	 * reuse this capability entry to create _multiple_ ptes.  We
	 * won't create captbl entries for each of them, so they
	 * cannot be aliased/removed later.  The only adverse
	 * side-effect I can think of from this is that we cannot
	 * reclaim all of the boot-time memory, but that is so far
	 * into the future, I don't think we care.
	 */
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, ptecap, NULL, 1)) assert(0);
	pte_cap = (struct cap_pgtbl*)captbl_lkup(ct, ptecap);
	assert(pte_cap);

	/* Hook in the PTEs */
	for (i = 0 ; i < nptes ; i++) {
		u8_t   *p  = ptes + i * PAGE_SIZE;
		paddr_t pf = chal_va2pa(p);

		pgtbl_init_pte(p);
		pte_cap->pgtbl = (pgtbl_t)p;

		/* hook the pte into the boot component's page tables */
		ret = cap_cons(ct, BOOT_CAPTBL_SELF_PT, ptecap, (capid_t)(user_vaddr + i*PGD_RANGE));
		assert(!ret);
	}

	printk("\tMapping in %s.\n", label);
	/* Map in the actual memory. */
	for (i = 0 ; i < range/PAGE_SIZE ; i++) {
		u8_t *p     = kern_vaddr + i * PAGE_SIZE;
		paddr_t pf  = chal_va2pa(p);
		u32_t mapat = (u32_t)user_vaddr + i * PAGE_SIZE, flags = 0;

                if (uvm  && pgtbl_mapping_add(pgtbl, mapat, pf, PGTBL_USER_DEF)) assert(0);
                if (!uvm && pgtbl_cosframe_add(pgtbl, mapat, pf, PGTBL_COSFRAME)) assert(0);
                assert((void*)p == pgtbl_lkup(pgtbl, user_vaddr+i*PAGE_SIZE, &flags));
	}

	return 0;
}

/* FIXME:  loops to create threads/tcaps/rcv caps per core. */
static void
kern_boot_thd(struct captbl *ct, void *thd_mem, void *tcap_mem)
{
	struct cos_cpu_local_info *cos_info = cos_cpu_local_info();
	struct thread *t = thd_mem;
	struct tcap *tc = tcap_mem;
	int ret;

	assert(sizeof(struct cos_cpu_local_info) == STK_INFO_SZ);
	memset(cos_info, 0, sizeof(struct cos_cpu_local_info));
	cos_info->cpuid          = 0;
	cos_info->invstk_top     = 0;
	cos_info->overflow_check = 0xDEADBEEF;

	ret = thd_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITTHD_BASE,
			   thd_mem, BOOT_CAPTBL_SELF_COMP, 0);
	assert(!ret);

	ret = tcap_split(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITTCAP_BASE,
			 tcap_mem, 0 /* no source tcap */, 1, 1);
	tc->budget.cycles = TCAP_RES_INF; /* father time's got all the time in the world */
	assert(!ret);
	thd_current_update(t, tcap_mem, t, cos_cpu_local_info());

	ret = arcv_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITRCV_BASE,
			    BOOT_CAPTBL_SELF_COMP, BOOT_CAPTBL_SELF_INITTHD_BASE,
			    BOOT_CAPTBL_SELF_INITTCAP_BASE, 0, 1);
	assert(!ret);

	printk("\tCreating initial threads, tcaps, and rcv end-points in boot-component.\n");
}

void
kern_boot_comp(void)
{
        int ret = 0, nkmemptes;
        struct captbl *ct;
        unsigned int i;
	u8_t *boot_comp_captbl;
	pgtbl_t pgtbl = (pgtbl_t)chal_va2pa(&boot_comp_pgd);
	void *thd_mem, *tcap_mem;
	u32_t hw_bitmap = 0xFFFFFFFF;

	printk("Setting up the booter component.\n");

	boot_comp_captbl = mem_boot_alloc(BOOT_CAPTBL_NPAGES);
        ct = captbl_create(boot_comp_captbl);
        assert(ct);

        /* expand the captbl to use multiple pages. */
        for (i = PAGE_SIZE ; i < BOOT_CAPTBL_NPAGES*PAGE_SIZE ; i += PAGE_SIZE) {
                captbl_init(boot_comp_captbl + i, 1);
                ret = captbl_expand(ct, (i - PAGE_SIZE/2)/CAPTBL_LEAFSZ, captbl_maxdepth(), boot_comp_captbl + i);
                assert(!ret);
                captbl_init(boot_comp_captbl + PAGE_SIZE + PAGE_SIZE/2, 1);
                ret = captbl_expand(ct, i/CAPTBL_LEAFSZ, captbl_maxdepth(), boot_comp_captbl + i + PAGE_SIZE/2);
                assert(!ret);
        }

	thd_mem  = mem_boot_alloc(1);
	tcap_mem = mem_boot_alloc(1);
	assert(thd_mem && tcap_mem);
        if (captbl_activate_boot(ct, BOOT_CAPTBL_SELF_CT)) assert(0);
        if (sret_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SRET)) assert(0);
        if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT, pgtbl, 0)) assert(0);

	hw_asndcap_init();
	if (hw_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITHW_BASE, hw_bitmap)) assert(0);

	printk("\tCapability table and page-table created.\n");

	ret = boot_pgtbl_mappings_add(ct, pgtbl, BOOT_CAPTBL_BOOTVM_PTE, "booter VM", mem_bootc_start(),
				      (unsigned long)mem_bootc_vaddr(), mem_bootc_end() - mem_bootc_start(), 1);
	assert(ret == 0);

	/*
	 * This _must_ be the last allocation.  The bump pointer
	 * modifies this allocation.
	 *
	 * Need to account for the pages that will be allocated as
	 * PTEs
	 */
	nkmemptes = boot_nptes(mem_utmem_end() - mem_boot_end());
	ret = boot_pgtbl_mappings_add(ct, pgtbl, BOOT_CAPTBL_KM_PTE, "untyped memory", mem_boot_nalloc_end(nkmemptes),
				      BOOT_MEM_KM_BASE, mem_utmem_end() - mem_boot_nalloc_end(nkmemptes), 0);
	assert(ret == 0);
	/* Shut off further bump allocations */
	glb_memlayout.allocs_avail = 0;

	if (comp_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP, BOOT_CAPTBL_SELF_CT,
			  BOOT_CAPTBL_SELF_PT, 0, (vaddr_t)mem_bootc_entry(), NULL)) assert(0);
	printk("\tCreated boot component structure from page-table and capability-table.\n");

	kern_boot_thd(ct, thd_mem, tcap_mem);

	printk("\tBoot component initialization complete.\n");
}

void
kern_boot_upcall(void)
{
	u8_t *entry = mem_bootc_entry();
	u32_t flags = 0;
	void *p;

	printk("Upcall into boot component at ip 0x%x\n", entry);
	printk("------------------[ Kernel boot complete ]------------------\n");
	chal_user_upcall(entry, thd_current(cos_cpu_local_info())->tid, get_cpuid());
	assert(0); 		/* should never get here! */
}
