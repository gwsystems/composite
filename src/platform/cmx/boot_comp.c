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

int boot_pgtbl_mappings_add(struct captbl *ct, capid_t pgdcap)
{
	u32_t count;
	struct pgtbl* MPU_meta;
	int ret;
	u8_t *ptes;
	unsigned int nptes = 0, i;
	struct cap_pgtbl *pte_cap, *pgd_cap;
	pgtbl_t pgtbl;

	pgd_cap = (struct cap_pgtbl*)captbl_lkup(ct, pgdcap);
	if (!pgd_cap || !CAP_TYPECHK(pgd_cap, CAP_PGTBL)) assert(0);
	pgtbl = (pgtbl_t)&(pgd_cap->pgtbl);

	/* Add kernel data regions - they shall never be retyped into anything else */
	pgtbl->data.pgt[0]=0;
	pgtbl->data.pgt[1]=0;
	pgtbl->data.pgt[2]=0;
	pgtbl->data.pgt[3]=COS_CMX_FIRST_PAGE|PGTBL_USER_DEF;
	pgtbl->data.pgt[4]=COS_CMX_SECOND_PAGE|PGTBL_COSFRAME;
	pgtbl->data.pgt[5]=COS_CMX_THIRD_PAGE|PGTBL_COSFRAME;
	pgtbl->data.pgt[6]=COS_CMX_FOURTH_PAGE|PGTBL_COSFRAME;
	pgtbl->data.pgt[7]=COS_CMX_FIFTH_PAGE|PGTBL_COSFRAME;

	/* Retype the first component's range to user memory */
	for(unsigned long i=COS_CMX_FIRST_PAGE;i<COS_CMX_SECOND_PAGE;i+=RETYPE_MEM_SIZE)
		assert(retypetbl_retype2user((void*)i)==0);

	/* Manually construct the initial MPU settings. After this, all the settings must be constructed by the update function.
	 * here, we only grant access to the first MPU regions. All other code regions, by default, are enabled; and the background
	 * region is also enabled. These are done by default */
	chal_mpu_update(pgtbl, 1);
	return 0;
}

/* FIXME:  loops to create threads/tcaps/rcv caps per core. */
static void
kern_boot_thd(struct captbl *ct, void *thd_mem, void *tcap_mem)
{
	struct cos_cpu_local_info *cos_info = cos_cpu_local_info();
	struct thread *t = thd_mem;
	struct tcap   *tc = tcap_mem;
	tcap_res_t     expended;
	int            ret;
	struct cap_pgtbl *cap_pt;
	pgtbl_t           pgtbl;

	assert(sizeof(struct cos_cpu_local_info) == STK_INFO_SZ);
	memset(cos_info, 0, sizeof(struct cos_cpu_local_info));
	cos_info->cpuid          = 0;
	cos_info->invstk_top     = 0;
	cos_info->overflow_check = 0xDEADBEEF;

	ret = thd_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITTHD_BASE,
			   thd_mem, BOOT_CAPTBL_SELF_COMP, 0);
	assert(!ret);

	tcap_active_init(cos_info);
	ret = tcap_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITTCAP_BASE, tcap_mem, TCAP_PRIO_MAX);
	assert(!ret);
	tc->budget.cycles = TCAP_RES_INF; /* Chronos's got all the time in the world */
	tc->perm_prio     = 0;
	tcap_setprio(tc, 0);              /* Chronos gets preempted by no one! */
	list_enqueue(&cos_info->tcaps, &tc->active_list); /* Chronos on the TCap active list */
	cos_info->tcap_uid  = 1;
	cos_info->cycles    = tsc();
	cos_info->curr_tcap = tc;

	thd_current_update(t, t, cos_info);

	ret = arcv_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITRCV_BASE,
			    BOOT_CAPTBL_SELF_COMP, BOOT_CAPTBL_SELF_INITTHD_BASE,
			    BOOT_CAPTBL_SELF_INITTCAP_BASE, 0, 1);
	assert(!ret);

	/*
	 * boot component's mapped into SELF_PT,
	 * switching to boot component's pgd
	 */
	cap_pt = (struct cap_pgtbl *)captbl_lkup(ct, BOOT_CAPTBL_SELF_PT_META);
	if (!cap_pt || !CAP_TYPECHK(cap_pt, CAP_PGTBL)) assert(0);
	/* PRY: modified to pass compilation */
	pgtbl = &(cap_pt->pgtbl);
	assert(pgtbl);
	pgtbl_update(pgtbl);

	printk("\tCreating initial threads, tcaps, and rcv end-points in boot-component.\n");
}

void
kern_boot_comp(void)
{
	int ret = 0, nkmemptes;
	struct captbl *ct;
	unsigned int i;
	u8_t *boot_comp_captbl;
	void *thd_mem, *tcap_mem;
	u32_t hw_bitmap = 0xFFFFFFFF;
	/* PRY:Allocate some memory for the booter component */
	printk("Setting up the booter component.\n");

	boot_comp_captbl = mem_boot_alloc(BOOT_CAPTBL_NPAGES);
	assert(boot_comp_captbl);
	ct               = captbl_create(boot_comp_captbl);
	assert(ct);

	thd_mem  = mem_boot_alloc(1);
	tcap_mem = mem_boot_alloc(1);
	assert(thd_mem && tcap_mem);

	if (captbl_activate_boot(ct, BOOT_CAPTBL_SELF_CT)) assert(0);
	if (sret_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SRET)) assert(0);

	hw_asndcap_init();
	if (hw_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITHW_BASE, hw_bitmap)) assert(0);

	/*
	 * separate pgd for boot component virtual memory
	 */
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT_META, COS_PGTBL_MPUMETA, 0x20000000, COS_PGTBL_PGSZ_256K, COS_PGTBL_PGNUM_2)) assert(0);
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT_TABLE,COS_PGTBL_INTERN, 0x20000000, COS_PGTBL_PGSZ_64K, COS_PGTBL_PGNUM_8)) assert(0);
	if (pgtbl_cons(ct, BOOT_CAPTBL_SELF_PT_META, BOOT_CAPTBL_SELF_PT_TABLE, 0)) assert(0);

	/* We simply add all the user/untyped memory mappings, and leave the kernel data regions outside */
	ret = boot_pgtbl_mappings_add(ct, BOOT_CAPTBL_SELF_PT_TABLE);
	assert(ret == 0);
	printk("\tCapability table and page-table created.\n");

	/* Shut off further bump allocations */
	glb_memlayout.allocs_avail = 0;

	if (comp_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP, BOOT_CAPTBL_SELF_CT,
			  BOOT_CAPTBL_SELF_PT_META, 0, (vaddr_t)mem_bootc_entry(), NULL)) assert(0);
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
	chal_user_upcall(entry, thd_current(cos_cpu_local_info())->tid);
	assert(0); 		/* should never get here! */
}
