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

#define CMX_FRT1_FLASH 0x08040000
#define CMX_FRT2_FLASH 0x08060000
#define CMX_FRT1_RAM   0x20050000
#define CMX_FRT2_RAM   0x20060000

/* These are used to setup components */
struct cmx_comp_header
{
	unsigned int* data_load;
	unsigned int* data_start;
	unsigned int* data_end;
	unsigned int* bss_start;
	unsigned int* bss_end;
	/* The entry of the stack(not the actual function!!) */
	unsigned int* cos_upcall_stack;
	/* The entry of the sinv stack(not the actual function!!) */
	unsigned int* cos_sinv_stack;
};

void*
cos_cmx_loadcomp(struct cmx_comp_header* header)
{
	u32_t* src;
	u32_t* ptr;
	/* data section initialization */
	src=header->data_load;
	for(ptr=header->data_start;ptr<header->data_end;ptr++,src++)
		*ptr=*src;
	/* bss section initialization */
	for(ptr=header->bss_start;ptr<header->bss_end;ptr++)
		*ptr=0;
	return (void*)(header->cos_upcall_stack);
}

int
frt_pgtbl_mappings_add(struct captbl *ct, capid_t shpgdcap, capid_t pgdcap, capid_t uvmpgdcap, unsigned long addr)
{
	u32_t count;
	struct pgtbl* MPU_meta;
	int ret;
	u8_t *ptes;
	unsigned int nptes = 0, i;
	struct cap_pgtbl *pgd_cap, *uvmpgd_cap, *shpgd_cap;
	pgtbl_t pgtbl, uvmpgtbl, shpgtbl;

	/* Setup the untyped 4k memory segments first */
	uvmpgd_cap = (struct cap_pgtbl*)captbl_lkup(ct, uvmpgdcap);
	if (!uvmpgd_cap || !CAP_TYPECHK(uvmpgd_cap, CAP_PGTBL)) assert(0);
	uvmpgtbl = (pgtbl_t)&(uvmpgd_cap->pgtbl);

	uvmpgtbl->data.pgt[0]=(addr+0x8000)|CMX_PGTBL_COSFRAME;
	uvmpgtbl->data.pgt[1]=(addr+0x8000+0x1000)|CMX_PGTBL_COSFRAME;
	uvmpgtbl->data.pgt[2]=(addr+0x8000+0x2000)|CMX_PGTBL_COSFRAME;
	uvmpgtbl->data.pgt[3]=(addr+0x8000+0x3000)|CMX_PGTBL_COSFRAME;
	uvmpgtbl->data.pgt[4]=(addr+0x8000+0x4000)|CMX_PGTBL_COSFRAME;
	uvmpgtbl->data.pgt[5]=(addr+0x8000+0x5000)|CMX_PGTBL_COSFRAME;
	uvmpgtbl->data.pgt[6]=(addr+0x8000+0x6000)|CMX_PGTBL_COSFRAME;
	uvmpgtbl->data.pgt[7]=(addr+0x8000+0x7000)|CMX_PGTBL_COSFRAME;

	/* Setup the freertos component's page table then */
	pgd_cap = (struct cap_pgtbl*)captbl_lkup(ct, pgdcap);
	if (!pgd_cap || !CAP_TYPECHK(pgd_cap, CAP_PGTBL)) assert(0);
	pgtbl = (pgtbl_t)&(pgd_cap->pgtbl);

	/* Add kernel data regions - they shall never be retyped into anything else */
	pgtbl->data.pgt[0]=addr|CMX_PGTBL_USER_DEF;
	/* pgtbl->data.pgt[1]=0; something cons'ed into here already */

	/* Retype the frt component's range to user memory */
	for(unsigned long i=addr;i<addr+0x8000;i+=RETYPE_MEM_SIZE)
		assert(retypetbl_retype2user((void*)i)==0);

	/* Manually construct the initial MPU settings. After this, all the settings must be constructed by the update function.
	 * here, we only grant access to the first MPU regions. All other code regions, by default, are enabled; and the background
	 * region is also enabled. These are done by default */
	chal_mpu_update(pgtbl, 1);

	/* Setup the untyped 4k memory segments first */
	shpgd_cap = (struct cap_pgtbl*)captbl_lkup(ct, shpgdcap);
	if (!shpgd_cap || !CAP_TYPECHK(shpgd_cap, CAP_PGTBL)) assert(0);
	shpgtbl = (pgtbl_t)&(shpgd_cap->pgtbl);

	shpgtbl->data.pgt[0]=0;
	shpgtbl->data.pgt[1]=(0x20029000)|CMX_PGTBL_USER_DEF;
	shpgtbl->data.pgt[2]=0;
	shpgtbl->data.pgt[3]=0;
	shpgtbl->data.pgt[4]=0;
	shpgtbl->data.pgt[5]=0;
	shpgtbl->data.pgt[6]=0;
	shpgtbl->data.pgt[7]=0;
	pgtbl=(pgtbl_t)&(shpgd_cap->pgtbl);
	chal_mpu_update(pgtbl, 1);

	return 0;
}

int boot_pgtbl_mappings_add(struct captbl *ct, capid_t pgdcap, capid_t uvm4k0pgdcap, capid_t uvm4k1pgdcap)
{
	u32_t count;
	struct pgtbl* MPU_meta;
	int ret;
	u8_t *ptes;
	unsigned int nptes = 0, i;
	struct cap_pgtbl *pgd_cap, *uvm4k0pgd_cap, *uvm4k1pgd_cap;
	pgtbl_t pgtbl, uvm4k0pgtbl, uvm4k1pgtbl;

	/* Setup the untyped 4k memory segments first */
	uvm4k0pgd_cap = (struct cap_pgtbl*)captbl_lkup(ct, uvm4k0pgdcap);
	if (!uvm4k0pgd_cap || !CAP_TYPECHK(uvm4k0pgd_cap, CAP_PGTBL)) assert(0);
	uvm4k0pgtbl = (pgtbl_t)&(uvm4k0pgd_cap->pgtbl);

	uvm4k0pgtbl->data.pgt[0]=(COS_CMX_SECOND_PAGE+0x0000)|CMX_PGTBL_COSFRAME;
	uvm4k0pgtbl->data.pgt[1]=(COS_CMX_SECOND_PAGE+0x1000)|CMX_PGTBL_COSFRAME;
	uvm4k0pgtbl->data.pgt[2]=(COS_CMX_SECOND_PAGE+0x2000)|CMX_PGTBL_COSFRAME;
	uvm4k0pgtbl->data.pgt[3]=(COS_CMX_SECOND_PAGE+0x3000)|CMX_PGTBL_COSFRAME;
	uvm4k0pgtbl->data.pgt[4]=(COS_CMX_SECOND_PAGE+0x4000)|CMX_PGTBL_COSFRAME;
	uvm4k0pgtbl->data.pgt[5]=(COS_CMX_SECOND_PAGE+0x5000)|CMX_PGTBL_COSFRAME;
	uvm4k0pgtbl->data.pgt[6]=(COS_CMX_SECOND_PAGE+0x6000)|CMX_PGTBL_COSFRAME;
	uvm4k0pgtbl->data.pgt[7]=(COS_CMX_SECOND_PAGE+0x7000)|CMX_PGTBL_COSFRAME;

	uvm4k1pgd_cap = (struct cap_pgtbl*)captbl_lkup(ct, uvm4k1pgdcap);
	if (!uvm4k1pgd_cap || !CAP_TYPECHK(uvm4k1pgd_cap, CAP_PGTBL)) assert(0);
	uvm4k1pgtbl = (pgtbl_t)&(uvm4k1pgd_cap->pgtbl);

	uvm4k1pgtbl->data.pgt[0]=(COS_CMX_SECOND_PAGE+0x8000)|CMX_PGTBL_COSFRAME;
	uvm4k1pgtbl->data.pgt[1]=(COS_CMX_SECOND_PAGE+0x9000)|CMX_PGTBL_COSFRAME;
	uvm4k1pgtbl->data.pgt[2]=(COS_CMX_SECOND_PAGE+0xA000)|CMX_PGTBL_COSFRAME;
	uvm4k1pgtbl->data.pgt[3]=(COS_CMX_SECOND_PAGE+0xB000)|CMX_PGTBL_COSFRAME;
	uvm4k1pgtbl->data.pgt[4]=(COS_CMX_SECOND_PAGE+0xC000)|CMX_PGTBL_COSFRAME;
	uvm4k1pgtbl->data.pgt[5]=(COS_CMX_SECOND_PAGE+0xD000)|CMX_PGTBL_COSFRAME;
	uvm4k1pgtbl->data.pgt[6]=(COS_CMX_SECOND_PAGE+0xE000)|CMX_PGTBL_COSFRAME;
	uvm4k1pgtbl->data.pgt[7]=(COS_CMX_SECOND_PAGE+0xF000)|CMX_PGTBL_COSFRAME;

	/* Setup the booter component's page table then */
	pgd_cap = (struct cap_pgtbl*)captbl_lkup(ct, pgdcap);
	if (!pgd_cap || !CAP_TYPECHK(pgd_cap, CAP_PGTBL)) assert(0);
	pgtbl = (pgtbl_t)&(pgd_cap->pgtbl);

	/* Add kernel data regions - they shall never be retyped into anything else */
	pgtbl->data.pgt[0]=0;
	pgtbl->data.pgt[1]=0;
	pgtbl->data.pgt[2]=0;
	pgtbl->data.pgt[3]=COS_CMX_FIRST_PAGE|CMX_PGTBL_USER_DEF;
	/* pgtbl->data.pgt[4]=0; something cons'ed into here already */
	/* This part is erased from our sight already
	pgtbl->data.pgt[5]=COS_CMX_THIRD_PAGE|PGTBL_COSFRAME;
	pgtbl->data.pgt[6]=COS_CMX_FOURTH_PAGE|PGTBL_COSFRAME;
	pgtbl->data.pgt[7]=COS_CMX_FIFTH_PAGE|PGTBL_COSFRAME; */

	/* Retype the first component's range to user memory */
	for(unsigned long i=COS_CMX_FIRST_PAGE;i<COS_CMX_SECOND_PAGE;i+=RETYPE_MEM_SIZE)
		assert(retypetbl_retype2user((void*)i)==0);

	/* Manually construct the initial MPU settings. After this, all the settings must be constructed by the update function.
	 * here, we only grant access to the first MPU regions. All other code regions, by default, are enabled; and the background
	 * region is also enabled. These are done by default */
	chal_mpu_update(pgtbl, 1);
	return 0;
}

/* PRY:we have two chronos now - this shall be fixed later */
static void
kern_boot_frt(struct captbl *ct, void *thd_mem, void *tcap_mem)
{
	struct thread *t = thd_mem;
	struct tcap   *tc = tcap_mem;
	tcap_res_t     expended;
	int            ret;
	struct cap_pgtbl *cap_pt;
	pgtbl_t           pgtbl;

	//ret = thd_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITTHD_BASE,
	//		   thd_mem, BOOT_CAPTBL_SELF_COMP, 0);
	//assert(!ret);
	/* The issue might be the tcap. we can try to delegate the master's created tcap to its own captbl instead of creating another one */
//	ret = tcap_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITTCAP_BASE, tcap_mem);
//	assert(!ret);
//	/* PRY:Now we have two chronos, This is incorrect. fix this later. We need somehow delegate time to this guy */
//	tc->budget.cycles = TCAP_RES_INF; /* Chronos's got all the time in the world - try this again */
//	tc->perm_prio     = 0;
//	tcap_setprio(tc, 0);              /* Chronos gets preempted by no one! */
	/*
	ret = arcv_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITRCV_BASE,
			    BOOT_CAPTBL_SELF_COMP, BOOT_CAPTBL_SELF_INITTHD_BASE,
			    BOOT_CAPTBL_SELF_INITTCAP_BASE, 0, 1);*/
	//assert(!ret);

	/*
	 * boot component's mapped into SELF_PT,
	 * switching to boot component's pgd
	 */
	cap_pt = (struct cap_pgtbl *)captbl_lkup(ct, BOOT_CAPTBL_SELF_PT_META);
	if (!cap_pt || !CAP_TYPECHK(cap_pt, CAP_PGTBL)) assert(0);
	/* PRY: modified to pass compilation */
	pgtbl = &(cap_pt->pgtbl);
	assert(pgtbl);
}

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
	ret = tcap_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITTCAP_BASE, tcap_mem);
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
extern int cap_cpy(struct captbl *t, capid_t cap_to, capid_t capin_to,
	           capid_t cap_from, capid_t capin_from);

void
kern_setup_frt(struct captbl* mct, struct cmx_comp_header* flashaddr,unsigned long ramaddr, int num)
{
	int ret = 0, nkmemptes;
	struct captbl *ct;
	unsigned int i;
	u8_t *boot_comp_captbl;
	void *thd_mem, *tcap_mem;
	u32_t hw_bitmap = 0xFFFFFFFF;

	/* This function will load the frt components and get them setup initially */
	printk("Setting up the frt component\n");
	boot_comp_captbl = mem_boot_alloc(BOOT_CAPTBL_NPAGES);
	assert(boot_comp_captbl);
	ct               = captbl_create(boot_comp_captbl);
	assert(ct);

	//thd_mem  = mem_boot_alloc(1);
	//tcap_mem = mem_boot_alloc(1);
	//assert(thd_mem && tcap_mem);
	/* This is activated in the booter component's captbl so we can call this from the booter! */
	if (captbl_activate(mct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_COMP1_CT+num*4,ct,0)) assert(0);
	/* Place the capability of the frt in frt's captbl as well */
	if(cap_cpy(mct, BOOT_CAPTBL_COMP1_CT+num*4, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_COMP1_CT+num*4)) assert(0);
	/* Place the capability of the master in frt's captbl as well */
	if(cap_cpy(mct, BOOT_CAPTBL_COMP1_CT+num*4, BOOT_CAPTBL_COMP1_CT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_CT)) assert(0);

	if (sret_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SRET)) assert(0);
//	/* This cannot access hw - we just hard-coded the cycles variable for them */
//	hw_asndcap_init();1
//	if (hw_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITHW_BASE, hw_bitmap)) assert(0);

	/* separate pgd for frt component virtual memory - how to? */
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT_META,COS_PGTBL_MPUMETA, 0x20000000, COS_PGTBL_PGSZ_64K, COS_PGTBL_PGNUM_8)) assert(0);
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_UNTYPED_PT_4K0,COS_PGTBL_INTERN, 0x20000000, COS_PGTBL_PGSZ_64K, COS_PGTBL_PGNUM_8)) assert(0);
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_UNTYPED_PT_4K1, COS_PGTBL_INTERN, 0x20028000, COS_PGTBL_PGSZ_4K, COS_PGTBL_PGNUM_8)) assert(0);
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT_TABLE,COS_PGTBL_INTERN, ramaddr, COS_PGTBL_PGSZ_32K, COS_PGTBL_PGNUM_2)) assert(0);
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_UNTYPED_PT,COS_PGTBL_INTERN, ramaddr+0x8000, COS_PGTBL_PGSZ_4K, COS_PGTBL_PGNUM_8)) assert(0);

	if (pgtbl_cons(ct, BOOT_CAPTBL_SELF_PT_META, BOOT_CAPTBL_SELF_UNTYPED_PT_4K0, 0)) assert(0);
	if (pgtbl_cons(ct, BOOT_CAPTBL_SELF_UNTYPED_PT_4K0, BOOT_CAPTBL_SELF_UNTYPED_PT_4K1, 2)) assert(0);
	if (pgtbl_cons(ct, BOOT_CAPTBL_SELF_UNTYPED_PT_4K0, BOOT_CAPTBL_SELF_PT_TABLE, num+5)) assert(0);
	if (pgtbl_cons(ct, BOOT_CAPTBL_SELF_PT_TABLE, BOOT_CAPTBL_SELF_UNTYPED_PT, 1)) assert(0);

	/* Map in all the pages */

	ret=frt_pgtbl_mappings_add(ct, BOOT_CAPTBL_SELF_UNTYPED_PT_4K1, BOOT_CAPTBL_SELF_PT_TABLE, BOOT_CAPTBL_SELF_UNTYPED_PT, ramaddr);

	/* We simply add all the user/untyped memory mappings, and leave the kernel data regions outside */
	assert(ret == 0);
	printk("\tCapability table and page-table created.\n");

	/* This is the frt component, and we also set it up */
	if (comp_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP, BOOT_CAPTBL_SELF_CT,
			  BOOT_CAPTBL_SELF_PT_META, 0, (vaddr_t)cos_cmx_loadcomp((struct cmx_comp_header*)flashaddr), NULL)) assert(0);

	/* Place the component capability in the master component as well */
	if(cap_cpy(ct, BOOT_CAPTBL_COMP1_CT, BOOT_CAPTBL_COMP1_COMP+num*4, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP)) assert(0);

	/* Create a synchronous invocation capability in the parent */
	sinv_activate(mct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_COMP1_INV+num*4,
			   BOOT_CAPTBL_COMP1_COMP+num*4, ((struct cmx_comp_header*)flashaddr)->cos_sinv_stack);

	/* This is creating for the boot-time frt component */
	kern_boot_frt(ct, thd_mem, tcap_mem);

	/* Place the booter's thread in frt so we can switch back */
	if(cap_cpy(mct, BOOT_CAPTBL_COMP1_CT+num*4, BOOT_CAPTBL_PARENT_INITTHD, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITTHD_BASE)) assert(0);

	printk("\tCreated boot component structure from page-table and capability-table.\n");
	printk("\tBoot component initialization complete.\n");
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
	 * separate pgd for boot component virtual memory - creating everything in the booter now
	 */
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT_META, COS_PGTBL_MPUMETA, 0x20000000, COS_PGTBL_PGSZ_256K, COS_PGTBL_PGNUM_2)) assert(0);
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT_TABLE,COS_PGTBL_INTERN, 0x20000000, COS_PGTBL_PGSZ_64K, COS_PGTBL_PGNUM_8)) assert(0);
	if (pgtbl_cons(ct, BOOT_CAPTBL_SELF_PT_META, BOOT_CAPTBL_SELF_PT_TABLE, 0)) assert(0);
	/* Activate these page tables for the booter component, and setup the page layout accordingly */
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_UNTYPED_PT, COS_PGTBL_INTERN, 0x20040000, COS_PGTBL_PGSZ_32K, COS_PGTBL_PGNUM_2)) assert(0);
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_UNTYPED_PT_4K0, COS_PGTBL_INTERN, 0x20040000, COS_PGTBL_PGSZ_4K, COS_PGTBL_PGNUM_8)) assert(0);
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_UNTYPED_PT_4K1, COS_PGTBL_INTERN, 0x20048000, COS_PGTBL_PGSZ_4K, COS_PGTBL_PGNUM_8)) assert(0);
	/* Construct the page table accordingly */
	///if (pgtbl_cons(ct, BOOT_CAPTBL_SELF_UNTYPED_PT_META, BOOT_CAPTBL_SELF_UNTYPED_PT, 0)) assert(0);
	if (pgtbl_cons(ct, BOOT_CAPTBL_SELF_PT_TABLE, BOOT_CAPTBL_SELF_UNTYPED_PT, 4)) assert(0);
	if (pgtbl_cons(ct, BOOT_CAPTBL_SELF_UNTYPED_PT, BOOT_CAPTBL_SELF_UNTYPED_PT_4K0, 0)) assert(0);
	if (pgtbl_cons(ct, BOOT_CAPTBL_SELF_UNTYPED_PT, BOOT_CAPTBL_SELF_UNTYPED_PT_4K1, 1)) assert(0);
	/* We simply add all the user/untyped memory mappings, and leave the kernel data regions outside */
	ret = boot_pgtbl_mappings_add(ct, BOOT_CAPTBL_SELF_PT_TABLE, BOOT_CAPTBL_SELF_UNTYPED_PT_4K0, BOOT_CAPTBL_SELF_UNTYPED_PT_4K1);
	assert(ret == 0);
	printk("\tCapability table and page-table created.\n");
	/* This is the booter component */
	if (comp_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP, BOOT_CAPTBL_SELF_CT,
			  BOOT_CAPTBL_SELF_PT_META, 0, /* The entry is a stack in fact */(vaddr_t)mem_bootc_entry(), NULL)) assert(0);
	/* This is creating for the boot-time first thread */
	kern_boot_thd(ct, thd_mem, tcap_mem);

	/* just place the IOM inside the scheduler for now. No more preliminary modifications. */

	/* We have memory range 0x30000 dedicated to booter related. We can do it in this way: */
	/* each component have 0x10000 memory, should be sufficient.
	memory ranges:
	booter/scheduler 0x30000-0x40000, incl. boot-time heap.
        20045000
	1st frt component 0x50000, 0x50000-0x57FFF data, 0x58000-0x5FFFF untyped
	2nd frt component 0x60000, 0x60000-0x67FFF data, 0x68000-0x6FFFF untyped */
	/* Set them up here */
	kern_setup_frt(ct, (struct cmx_comp_header*)0x08040000,CMX_FRT1_RAM, 0);
	kern_setup_frt(ct, (struct cmx_comp_header*)0x08060000,CMX_FRT2_RAM, 1);

	/* Activate the invocation component */

	/* After FRT setup, place the invocation capability in the first FRT component */
	cap_cpy(ct, BOOT_CAPTBL_COMP1_CT, BOOT_CAPTBL_COMP1_INV, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_COMP2_INV);

	/* Setup complete, shut off further bump allocations */
	glb_memlayout.allocs_avail = 0;
}

/* The scheduler component is just the first frt component. The task is simple, just delegate some tap to the frt components consecutively */
extern void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3);
void
kern_boot_upcall(void)
{
	u8_t *entry = (u8_t*)cos_upcall_fn/*mem_bootc_entry()*/;
	u32_t flags = 0;
	void *p;
	printk("Upcall into boot component at ip 0x%x\n", entry);
	printk("------------------[ Kernel boot complete ]------------------\n");
	chal_user_upcall(entry, thd_current(cos_cpu_local_info())->tid);
	assert(0); 		/* should never get here! */
}
