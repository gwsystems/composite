#include "assert.h"
#include "kernel.h"
#include "boot_comp.h"
#include "chal_cpu.h"
#include "chal/chal_proto.h"
#include "chal_pgtbl.h"
#include "mem_layout.h"
#include "string.h"
#include <pgtbl.h>
#include <thd.h>
#include <component.h>
#include <inv.h>
#include <hw.h>

extern u8_t *boot_comp_pgd;

void *         thd_mem[NUM_CPU], *tcap_mem[NUM_CPU];
struct captbl *glb_boot_ct;

int
boot_nptes(unsigned int sz)
{
	return round_up_to_pow2(sz, PGD_RANGE) / PGD_RANGE;
}

unsigned long boot_sram_heap = 0x80001000;

u8_t *
sram_boot_alloc(int npages)
{
	u8_t *        r = boot_sram_heap;
	unsigned long i;

	boot_sram_heap += npages * (PAGE_SIZE / sizeof(u8_t));
	for (i = (unsigned long)r; i < (unsigned long)boot_sram_heap; i += PAGE_SIZE) {
		if ((unsigned long)i % RETYPE_MEM_NPAGES == 0) {
			if (retypetbl_retype2kern((void *)chal_va2pa((void *)i), PAGE_ORDER)) {}
		}
	}

	memset((void *)r, 0, npages * (PAGE_SIZE / sizeof(u8_t)));

	return r;
}

u8_t *
sram_uvm_alloc(int npages)
{
	u8_t *        r = boot_sram_heap;
	unsigned long i;

	boot_sram_heap += npages * (PAGE_SIZE / sizeof(u8_t));
	for (i = (unsigned long)r; i < (unsigned long)boot_sram_heap; i += PAGE_SIZE) {
		if ((unsigned long)i % RETYPE_MEM_NPAGES == 0) {
			if (retypetbl_retype2user((void *)chal_va2pa((void *)i), PAGE_ORDER)) {}
		}
	}

	memset((void *)r, 0, npages * (PAGE_SIZE / sizeof(u8_t)));

	return r;
}


int
boot_pgtbl_mappings_add(struct captbl *ct, capid_t pgdcap, capid_t ptecap, const char *label, void *kern_vaddr,
                        unsigned long user_vaddr, unsigned int range, int uvm)
{
	int               ret;
	u8_t *            ptes;
	unsigned int      nptes = 0, i, nsmalls, offset;
	struct cap_pgtbl *pte_cap, *pgd_cap;
	pgtbl_t           pgtbl;
	int               first = 0;

	pgd_cap = (struct cap_pgtbl *)captbl_lkup(ct, pgdcap);
	if (!pgd_cap || !CAP_TYPECHK(pgd_cap, CAP_PGTBL)) assert(0);
	pgtbl = chal_va2pa((pgtbl_t)pgd_cap->pgtbl);

	/* No superpage support is implemented for Cortex-A */
	nsmalls = range / PAGE_SIZE;
	nptes   = nsmalls / 256 + 1;

	ptes = mem_boot_alloc(nptes);
	assert(ptes);

	printk("\tCreating %d %s PTEs for PGD @ 0x%x from [%x,%x) to [%x,%x).\n", nptes, label,
	       chal_pa2va((paddr_t)pgtbl), kern_vaddr, kern_vaddr + range, user_vaddr, user_vaddr + range);

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
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, ptecap, NULL, 1)) {
		printk("\tBoot pte error: uvm %d, might be 2-pass or more passes.\n");
		/* assert(0); */
	}
	pte_cap = (struct cap_pgtbl *)captbl_lkup(ct, ptecap);
	assert(pte_cap);

	/* Hook in the PTEs */
	for (i = 0; i < nptes; i++) {
		u8_t *  p  = ptes + i * PAGE_SIZE;
		paddr_t pf = chal_va2pa(p);

		pgtbl_init_pte(p);
		pte_cap->pgtbl = (pgtbl_t)p;

		/* hook the pte into the boot component's page tables */
		ret = cap_cons(ct, pgdcap, ptecap, (capid_t)(user_vaddr + i * PGD_RANGE));
		assert(!ret);

		if (uvm != 0) {
			/* also hook them into the shadow component's page table */
			ret = cap_cons(ct, BOOT_CAPTBL_COMP0_PT, ptecap, (capid_t)(user_vaddr + i * PGD_RANGE));
			assert(!ret);
		}
	}

	printk("\tMapping in %s, %d pages.\n", label, nsmalls);
	/* Map in the actual memory - align to 4MB first */
	for (i = 0; i < nsmalls; i++) {
		u8_t *  p  = kern_vaddr + i * PAGE_SIZE;
		paddr_t pf = chal_va2pa(p);

		u32_t mapat = (u32_t)user_vaddr + i * PAGE_SIZE, flags = 0;
		if (uvm && pgtbl_mapping_add(pgtbl, mapat, pf, CAV7_4K_USER_DEF, PAGE_ORDER)) assert(0);
		if (!uvm && pgtbl_cosframe_add(pgtbl, mapat, pf, CAV7_PGTBL_COSFRAME, PAGE_ORDER)) assert(0);

		assert((void *)p == pgtbl_lkup(pgtbl, user_vaddr + i * PAGE_SIZE, &flags));
	}


	/* Also, for the shadow component, create something and map it in. We are very
	 * clear that we can only make 15 entries due to the limited space of the MCU.
	 * user_vaddr  */
	if (uvm != 0) {
		user_vaddr = 0x1A000000;
		/* Allocate 14 ptes */
		ptes = sram_boot_alloc(14);
		for (i = 0; i < 14; i++) {
			u8_t *  p  = ptes + i * PAGE_SIZE;
			paddr_t pf = chal_va2pa(p);

			pgtbl_init_pte(p);
			pte_cap->pgtbl = (pgtbl_t)p;

			/* hook the pte into the boot component's page tables */
			ret = cap_cons(ct, BOOT_CAPTBL_COMP0_PT, ptecap, (capid_t)(user_vaddr + i * PGD_RANGE));
			assert(!ret);
		}

		pgd_cap = (struct cap_pgtbl *)captbl_lkup(ct, BOOT_CAPTBL_COMP0_PT);
		if (!pgd_cap || !CAP_TYPECHK(pgd_cap, CAP_PGTBL)) assert(0);
		pgtbl = chal_va2pa((pgtbl_t)pgd_cap->pgtbl);

		/* Map in the actual memory - first 16 pages are L2-speed based SRAM, the rest are SDRAM */
		ptes = sram_uvm_alloc(16);
		printk("\tMapping in SRAM %s, %d pages, vaddr %x.\n", label, 16, ptes);
		for (i = 0; i < 16; i++) {
			u8_t *  p     = 0x80010000 + i * PAGE_SIZE;
			paddr_t pf    = chal_va2pa(p);
			u32_t   mapat = (u32_t)user_vaddr + i * PAGE_SIZE, flags = 0;

			if (pgtbl_mapping_add(pgtbl, mapat, pf, CAV7_4K_USER_DEF, PAGE_ORDER)) assert(0);

			assert((void *)p == pgtbl_lkup(pgtbl, user_vaddr + i * PAGE_SIZE, &flags));
		}

		user_vaddr = 0x1A010000;
		for (i = 0; i < 13 * 256; i++) {
			u8_t *  p     = 0x9F000000 + i * PAGE_SIZE;
			paddr_t pf    = chal_va2pa(p);
			u32_t   mapat = (u32_t)user_vaddr + i * PAGE_SIZE, flags = 0;

			if (retypetbl_retype2user((void *)chal_va2pa((void *)p), PAGE_ORDER)) assert(0);
			if (pgtbl_mapping_add(pgtbl, mapat, pf, CAV7_4K_USER_DEF, PAGE_ORDER)) assert(0);

			assert((void *)p == pgtbl_lkup(pgtbl, user_vaddr + i * PAGE_SIZE, &flags));
		}
	}


	return 0;
}

/* FIXME:  loops to create threads/tcaps/rcv caps per core. */
static void
kern_boot_thd(struct captbl *ct, void *thd_mem, void *tcap_mem, const cpuid_t cpu_id)
{
	struct cos_cpu_local_info *cos_info = cos_cpu_local_info();
	struct thread *            t        = thd_mem;
	struct tcap *              tc       = tcap_mem;
	tcap_res_t                 expended;
	int                        ret;
	struct cap_pgtbl *         cap_pt;
	pgtbl_t                    pgtbl;

	assert(cpu_id >= 0);
	assert(cos_info->cpuid == (u32_t)cpu_id);

	assert(sizeof(struct cos_cpu_local_info) == STK_INFO_SZ);
	memset(cos_info, 0, sizeof(struct cos_cpu_local_info));
	cos_info->cpuid          = cpu_id;
	cos_info->invstk_top     = 0;
	cos_info->overflow_check = 0xDEADBEEF;
	ret = thd_activate(ct, BOOT_CAPTBL_SELF_CT, captbl_thd_offset(cpu_id), thd_mem, BOOT_CAPTBL_SELF_COMP, 0);
	assert(!ret);

	tcap_active_init(cos_info);
	ret = tcap_activate(ct, BOOT_CAPTBL_SELF_CT, captbl_tcap_offset(cpu_id), tcap_mem);
	assert(!ret);

	tc->budget.cycles = TCAP_RES_INF; /* Chronos's got all the time in the world */
	tc->perm_prio     = 0;
	tcap_setprio(tc, 0);                              /* Chronos gets preempted by no one! */
	list_enqueue(&cos_info->tcaps, &tc->active_list); /* Chronos on the TCap active list */
	cos_info->tcap_uid  = 1;
	cos_info->cycles    = tsc();
	cos_info->curr_tcap = tc;
	thd_next_thdinfo_update(cos_info, 0, 0, 0, 0);

	thd_current_update(t, t, cos_info);
	thd_scheduler_set(t, t);

	ret = arcv_activate(ct, BOOT_CAPTBL_SELF_CT, captbl_arcv_offset(cpu_id), BOOT_CAPTBL_SELF_COMP,
	                    captbl_thd_offset(cpu_id), captbl_tcap_offset(cpu_id), 0, 1);
	assert(!ret);

	/*
	 * boot component's mapped into SELF_PT,
	 * switching to boot component's pgd
	 */
	cap_pt = (struct cap_pgtbl *)captbl_lkup(ct, BOOT_CAPTBL_SELF_PT);
	if (!cap_pt || !CAP_TYPECHK(cap_pt, CAP_PGTBL)) assert(0);
	pgtbl = cap_pt->pgtbl;
	assert(pgtbl);
	pgtbl_update(pgtbl);

	printk("content %x\n", *((volatile unsigned long *)0x98003400));
	printk("content2 %x\n", *((volatile unsigned long *)0x98004000));
	printk("content3 %x\n", *((volatile unsigned long *)0x10000000));

	printk("\tCreating initial threads, tcaps, and rcv end-points in boot-component.\n");
}

void
kern_boot_comp(const cpuid_t cpu_id)
{
	int          ret = 0, nkmemptes;
	unsigned int i;
	u8_t *       boot_comp_captbl;
	pgtbl_t      pgtbl     = (pgtbl_t)chal_va2pa(&boot_comp_pgd), boot_vm_pgd, sram_vm_pgd;
	u32_t        hw_bitmap = 0xFFFFFFFF;

	/* I don't need this ...
	assert(cpu_id >= 0);
	if (NUM_CPU > 1 && cpu_id > 0) {
	        assert(glb_boot_ct);
	        pgtbl_update(pgtbl);
	        kern_boot_thd(glb_boot_ct, thd_mem[cpu_id], tcap_mem[cpu_id], cpu_id);
	        return;
	} */

	printk("Setting up the booter component.\n");

	boot_comp_captbl = mem_boot_alloc(BOOT_CAPTBL_NPAGES);
	assert(boot_comp_captbl);
	glb_boot_ct = captbl_create(boot_comp_captbl);
	assert(glb_boot_ct);

	/* expand the captbl to use multiple pages. */
	for (i = PAGE_SIZE; i < BOOT_CAPTBL_NPAGES * PAGE_SIZE; i += PAGE_SIZE) {
		captbl_init(boot_comp_captbl + i, 1);
		ret = captbl_expand(glb_boot_ct, (i - PAGE_SIZE / 2) / CAPTBL_LEAFSZ, captbl_maxdepth(),
		                    boot_comp_captbl + i);
		assert(!ret);
		captbl_init(boot_comp_captbl + PAGE_SIZE + PAGE_SIZE / 2, 1);
		ret = captbl_expand(glb_boot_ct, i / CAPTBL_LEAFSZ, captbl_maxdepth(),
		                    boot_comp_captbl + i + PAGE_SIZE / 2);
		assert(!ret);
	}

	for (i = 0; i < NUM_CPU; i++) {
		thd_mem[i]  = mem_boot_alloc(1);
		tcap_mem[i] = mem_boot_alloc(1);
		assert(thd_mem[i] && tcap_mem[i]);
	}

	if (captbl_activate_boot(glb_boot_ct, BOOT_CAPTBL_SELF_CT)) assert(0);
	if (sret_activate(glb_boot_ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SRET)) assert(0);

	hw_asndcap_init();
	if (hw_activate(glb_boot_ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITHW_BASE, hw_bitmap)) assert(0);

	/*
	 * separate pgd for boot component virtual memory
	 */
	boot_vm_pgd = (pgtbl_t)mem_boot_alloc(1);
	printk("boot vm pgd %x, capid %d\n", boot_vm_pgd, BOOT_CAPTBL_SELF_PT);
	assert(boot_vm_pgd);
	/* There's no need to copy any kernel entry into this - because we know that kernel uses TTBR1 */
	if (pgtbl_activate(glb_boot_ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT, (pgtbl_t)boot_vm_pgd, 0)) assert(0);
	/*
	 * separate pgd for shadow booter
	 */
	sram_vm_pgd = sram_boot_alloc(1);
	printk("SRAM vm pgd %x, capid %d\n", sram_vm_pgd, BOOT_CAPTBL_COMP0_PT);
	assert(sram_vm_pgd);
	if (pgtbl_activate(glb_boot_ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_COMP0_PT, (pgtbl_t)sram_vm_pgd, 0)) assert(0);


	/* This is the booter component - we also make a shadow booter component that replicates the static segments but
	 * have all its dynamic memory mapping page tables in SRAM. */
	ret = boot_pgtbl_mappings_add(glb_boot_ct, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_BOOTVM_PTE, "booter VM",
	                              mem_bootc_start(), (unsigned long)mem_bootc_vaddr(),
	                              mem_bootc_end() - mem_bootc_start(), 1);
	assert(ret == 0);

	/*
	 * This _must_ be the last allocation.  The bump pointer
	 * modifies this allocation.
	 *
	 * Need to account for the pages that will be allocated as
	 * PTEs
	 */
	if (pgtbl_activate(glb_boot_ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_UNTYPED_PT, (pgtbl_t)&boot_comp_pgd, 0))
		assert(0);
	/* We assume 128 pages for SRAM */
	nkmemptes = boot_nptes(mem_utmem_end() - mem_boot_end());
	ret = boot_pgtbl_mappings_add(glb_boot_ct, BOOT_CAPTBL_SELF_UNTYPED_PT, BOOT_CAPTBL_KM_PTE, "untyped memory",
	                              mem_boot_nalloc_end(nkmemptes), BOOT_MEM_KM_BASE,
	                              mem_utmem_end() - mem_boot_nalloc_end(nkmemptes), 0);
	assert(ret == 0);

	/* Add the OCSRAM into the page table, also, at a lower address range
	ret       = boot_pgtbl_mappings_add(glb_boot_ct, BOOT_CAPTBL_SELF_UNTYPED_PT, BOOT_CAPTBL_KM_PTE, "untyped
	SRAM", 0x1000, 0x30000000, 47*0x1000, 0);  */
	assert(ret == 0);


	printk("\tCapability table and page-table created.\n");

	/* Shut off further bump allocations */
	glb_memlayout.allocs_avail = 0;
	/* This is the main component */
	if (comp_activate(glb_boot_ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP, BOOT_CAPTBL_SELF_CT,
	                  BOOT_CAPTBL_SELF_PT, 0, (vaddr_t)mem_bootc_entry(), NULL))
		assert(0);
	if (comp_activate(glb_boot_ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_COMP0_COMP, BOOT_CAPTBL_SELF_CT,
	                  BOOT_CAPTBL_COMP0_PT, 0, (vaddr_t)mem_bootc_entry(), NULL))
		assert(0);
	printk("\tCreated boot component structure from page-table and capability-table, entry at %x.\n",
	       (vaddr_t)mem_bootc_entry());

	kern_boot_thd(glb_boot_ct, thd_mem[cpu_id], tcap_mem[cpu_id], cpu_id);

	printk("\tBoot component initialization complete.\n");
}

void
kern_boot_upcall(void)
{
	u8_t *entry = mem_bootc_entry();
	u32_t flags = 0;
	void *p;

	assert(get_cpuid() >= 0);
	printk("Upcall into boot component at ip 0x%x for cpu: %d with tid: %d\n", entry, get_cpuid(),
	       thd_current(cos_cpu_local_info())->tid);
	/* only print complete msg for BSP */
	if (get_cpuid() == 0) { printk("------------------[ Kernel boot complete ]------------------\n"); }

	chal_user_upcall(entry, thd_current(cos_cpu_local_info())->tid, get_cpuid());
	assert(0); /* should never get here! */
}
