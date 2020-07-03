/**
 * Hijack, or Asymmetric Execution Domains support for Linux
 *
 * Copyright 2007 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2007
 */

#ifndef CONFIG_X86_LOCAL_APIC
#define CONFIG_X86_LOCAL_APIC
#endif

#include <linux/module.h>
//#include <linux/config.h>
#include <linux/init.h>
#include <linux/sched.h>
//#include <linux/interrupt.h> /* cli/sti */
#include <linux/proc_fs.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/errno.h>
/* global_flush_tlb */
#include <asm/cacheflush.h>
#include <asm/apic.h>
#include <asm/ipi.h>
/* fget */
#include <linux/file.h>
/* smp functions */
#include <linux/smp.h>
/* all page table manipulations */
#include <asm/pgtable.h>
/* (rd|wr)msr */
#include <asm/msr.h>

#include <asm/mmu_context.h>

/* for asm-generic/irq_regs.h:get_irq_regs */
#include <linux/irq.h>
//#include <linux/timer.h>

#include "aed_ioctl.h"
#include "asym_exec_dom.h"

#include "../../../kernel/include/spd.h"
#include "../../../kernel/include/ipc.h"
#include "../../../kernel/include/thread.h"
#include "../../../kernel/include/measurement.h"
#include "../../../kernel/include/mmap.h"
#include "../../../kernel/include/per_cpu.h"
#include "../../../kernel/include/shared/consts.h"
#include "../../../kernel/include/shared/cos_config.h"
#include "../../../kernel/include/fpu.h"
#include "../../../kernel/include/chal/cpuid.h"
#include "../../../kernel/include/asm_ipc_defs.h"

#include "./hw_ints.h"
#include "cos_irq_vectors.h"

#include "linux_pgtbl.h"

#include "../../../kernel/include/chal.h"
#include "../../../kernel/include/pgtbl.h"
#include "../../../kernel/include/captbl.h"
#include "../../../kernel/include/cap_ops.h"
#include "../../../kernel/include/component.h"
#include "../../../kernel/include/inv.h"
#include "../../../kernel/include/thd.h"
#include "../../../kernel/include/retype_tbl.h"

#include "./kconfig_checks.h"

MODULE_LICENSE("GPL");
#define MODULE_NAME "asymmetric_execution_domain_support"

extern void sysenter_interposition_entry(void);
extern void page_fault_interposition(void);
extern void div_fault_interposition(void);
extern void reg_save_interposition(void);
extern void fpu_not_available_interposition(void);
extern void ipi_handler(void);
extern void timer_interposition(void);

/*
 * This variable exists for the assembly code for temporary
 * storage...want it close to sysenter_addr for cache locality
 * purposes. -> Replaced by the per_cpu variable x86_tss.
 */
extern unsigned long temp_esp_storage;

/*
 * This variable is the page table entry that links to all of the
 * shared region data including the read-only information page and the
 * data region for passing arguments and holding persistent data.
 */
pte_t *shared_region_pte;
pgd_t *union_pgd;

struct per_core_cos_thd cos_thd_per_core[NUM_CPU];

//Should this be per core? >>
struct mm_struct *composite_union_mm = NULL;

/*
 * These are really a per-thread resource (per CPU if we assume hijack
 * only runs one thread per CPU).  These data structures have been
 * taken out of the task struct to improve locality, and so that we
 * don't have to modify the linux kernel (one cannot patch
 * data-structures in some magic way in an already compiled kernel to
 * include extra fields).
 */
struct mm_struct *trusted_mm = NULL;

#define MAX_ALLOC_MM 64
struct mm_struct *guest_mms[MAX_ALLOC_MM];
//Should the above be per core? <<

DEFINE_PER_CPU(unsigned long, x86_tss) = { 0 };

/*
 * This function gets the TSS pointer from Linux.
 * We read it from Linux only once. After that, use the
 * get_TSS function below for efficiency.
 */
static inline void
load_per_core_TSS(void)
{
	unsigned long *cpu_x86_tss;
        struct tss_struct *gdt_tss = NULL;
	struct desc_struct *gdt_array = NULL;
	unsigned long *temp_gdt_tss;

	gdt_array = get_cpu_gdt_table(get_cpuid());
        temp_gdt_tss = (unsigned long *)get_desc_base(&gdt_array[GDT_ENTRY_TSS]);
	gdt_tss = (struct tss_struct *)temp_gdt_tss;

	cpu_x86_tss = &get_cpu_var(x86_tss);
	*cpu_x86_tss = (unsigned long)((void *)gdt_tss + sizeof(struct tss_struct));

	put_cpu_var(x86_tss);
}

/*
 * This function gets the TSS pointer of current CPU.
 * We load the TSS to a per CPU variable x86_tss when
 * we try getting it the first time. After that, we
 * can just load it from that variable.
 */
void get_TSS(struct pt_regs *rs)
{
	/* We pass the esp to this function from assembly.
	 * In the assembly code, we do SAVE_ALL before call
	 * this function. We also reserve a position in the stack
	 * before SAVE_ALL to receive the TSS from this function.
	 * So here, the orig_ax points right to that position.*/

	rs->orig_ax = get_cpu_var(x86_tss);
	if (unlikely(rs->orig_ax == 0)) {
		/* We need to load the variable if not loaded yet. */
	  	load_per_core_TSS();
		rs->orig_ax = get_cpu_var(x86_tss);
		/* Make sure the thread_info structure is at the
		   correct location. */
		assert(get_linux_thread_info() == (void *)current_thread_info());
	}

	return;
}

static void init_guest_mm_vect(void)
{
	int i;

	for (i = 0 ; i < MAX_ALLOC_MM ; i++) {
		guest_mms[i] = NULL;
	}

	return;
}

static int aed_find_empty_mm_handle(void)
{
	int i;

	for (i = 0 ; i < MAX_ALLOC_MM ; i++) {
		if (guest_mms[i] == NULL) {
			return i;
		}
	}

	return -1;
}

static inline struct mm_struct *aed_get_mm(int mm_handle)
{
	return guest_mms[mm_handle];
}

/*
 * This is needed when we are copying a current mm.  The copy process
 * cannot copy the current mm _to_ a given mm, only create a new copy
 * mm, and we want to replace the old stale mm with the new copy.  See
 * aed_ioctl and AED_COPY_MM.
 *
 * If there is no mm associated with mm_handle, return -1.
 */
static inline int aed_replace_mm(int mm_handle, struct mm_struct *new_mm)
{
	struct mm_struct *old_mm = guest_mms[mm_handle];

	if (!old_mm) {
		return -1;
	}

	guest_mms[mm_handle] = new_mm;

	mmput(old_mm);

	return 0;
}

/* mm creation/deletion functionality: */
static int aed_allocate_mm(void)
{
	struct mm_struct *mm = mm_alloc();
	int ret, mm_handle;

	ret = -ENOMEM;
	if(mm == NULL)
		goto out_mem;

	//init_new_empty_context(mm); see SKAS code, copied here:
	//init_MUTEX(&mm->context.sem);
	mutex_init(&mm->context.lock);
	mm->context.size = 0;

	arch_pick_mmap_layout(mm);

	mm_handle = aed_find_empty_mm_handle();

	if (mm_handle < 0)
		goto dealloc_mm;

	guest_mms[mm_handle] = mm;

	return mm_handle;

dealloc_mm:
	mmput(mm);
out_mem:
	return ret;
}

static int aed_free_mm(int mm_handle)
{
	struct mm_struct *mm = guest_mms[mm_handle];

	if (!mm) return 0;
	mmput(mm);
	guest_mms[mm_handle] = NULL;

	/* FIXME: -EBUSY if currently used mm...this would be solved
	 * if we were doing ref counting for when we switch to mm inc,
	 * switch away from dec, but we aren't cause that's in the
	 * fast path. */
	return 0;
}

int spd_free_mm(struct spd *spd)
{
	struct mm_struct *mm;
	pgd_t *pgd;
	int span;

	/* if the spd shares page tables with the creation program,
	 * don't kill anything */
	if (spd->location[0].size == 0) return 0;

	mm = guest_mms[spd->local_mmaps];
	pgd = pgd_offset(mm, spd->location[0].lowest_addr);
	span = hpage_index(spd->location[0].size);

	/* ASSUMPTION: we are talking about a component's mm here, so
	 * we need to remove the ptes */

	/* sizeof(pgd entry) is intended */
	memset(pgd, 0, span*sizeof(pgd_t));

	pgd = pgd_offset(mm, COS_INFO_REGION_ADDR);
	memset(pgd, 0, sizeof(pgd_t));

	aed_free_mm(spd->local_mmaps);

	return 0;
}

static void remove_all_guest_mms(void)
{
	int i;

	for (i = 0 ; i < MAX_ALLOC_MM ; i++) {
		if (guest_mms[i] == NULL) continue;

		aed_free_mm(i);
	}

	return;
}

#ifdef DEBUG
#define printkd(str,args...) printk(str, ## args)
#else
#define printkd(str,args...)
#endif

/* return a ptr to the register structure referring to the user-level regs */
static inline struct pt_regs* get_user_regs(void)
{
	/*
	Following calculations deduced from:

	in entry.S:

	pushl %esp
	call gp_print_esp
	popl %esp

	and elsewhere

	void gp_print_esp(unsigned int esp)
	{
	        printk("cosesp is %x; esp0 is %x; sizeof pt_regs %x.\n",
	               esp, (unsigned int)current->thread.esp0, (unsigned int)sizeof(struct pt_regs));
	        return;
	}
	*/

	return (struct pt_regs*)((int)current->thread.sp0 - sizeof(struct pt_regs));

	/*
	 * This is complicated because the user registers can be in
	 * one of three places.
	 *
	 * 1) User-level code was executing when an interrupt
	 * happened.  When we wish to retrieve the user-level
	 * interrupts, the kernel stack will look like:
	 *
	 * user_ss
	 * user_esp
	 * eflags
	 * user_cs
	 * user_ip
	 * ...
	 *
	 * Thus pt_regs can be used directly taken as an offset from esp0.
	 *
	 * 2) The interrupt can occur after the sti but before the
         * sysexit in ipc.S.  This is the only preemptible region in
         * the IPC fastpath.  In this case, the user-level registers
         * are the current register-set when the interrupt occurs.
         * This case can be detected because before the sti, a known
         * value (0 and 0, 2 words) is pushed onto the stack, thus at
         * the base of the stack is 0 in this case.  When the
         * interrupt occurs, it will create its own frame, thus the
         * stack will now look like:
	 *
	 * 0
	 * 0
	 * eflags
	 * user_cs
	 * user_ip
	 * ...
	 *
	 * Thus the same method for retrieving pt_regs can be used.
	 *
	 * 3) A system call could have been made by the composite
         * thread.  This is the painful case.  Because interrupts are
         * enabled before the user-registers are saved onto the stack,
         * the register contents can be spread across both the normal
         * syscall save area and an irq frame if one occurs before the
         * syscall finishes saving the registers.  If the registers are
         * not completely saved before the interrupt happens, then the
         * contents of get_irq_regs should hold the proper value
         * _unless_ we have nested irqs.  This implies that we cannot
         * access user-registers if we insert any code into interrupt
         * paths where set_irq_regs is used.  However, our saving grace
         * here is that softirqs don't have this problem.  Thus, if we
         * wish to set the user-regs, we have a conditional:
	 *
	 * if (syscall was interrupted before completely saving/restoring registers)
	 *    get_irq_regs holds user-level register contents (possibly not for restore)
	 * else
	 *    the normal offset from esp0 method should work
	 *
	 * Complication: eax is not set till syscall returns...
	 *
	 * a) Because of the complexity of this solution (#3), I am
	 * going to self-impose a restriction against making Linux
	 * syscalls for experiments using Composite.  Linux system
	 * calls are essentially non-preemptible sections, and should
	 * thus not be made in a RT setting.  A composite-specific
	 * system call can be added later to execute system calls in
	 * the context of another Linux thread, thus allowing the main
	 * composite thread to remain fully non-preemptive in kernel.
	 *
	 * b) A good approach might be to assume that all composite
	 * threads executing syscalls (i.e. if given permission to
	 * execute syscalls) should not be preempted.  We can tell if
	 * we are in the kernel when an interrupt happens by looking
	 * at the SS and esp on the stack...if not 0 and 0, don't
	 * preempt.  These is still a race as interrupts are reenabled
	 * in entry.S for sysenter before the DS (for SS) and user_esp
	 * are saved.  This can be eliminated by assuming that all
	 * system calls from within composite will use int 0x80 and
	 * iret probably from dietlibc.
	 *
	 * A sidenote:
	 * -----------
	 *
	 * Case (1) is affected by possible nested interrupts as well,
	 * and it is possible that we will be in a softirq that
	 * interrupted the saving/restoring of the user registers on
	 * the kernel stack.  This can be detected by checking if
	 * (get_user_regs - get_irq_regs) < sizeof(struct pt_regs).
	 * Selective rewriting of registers from both structures
	 * depending on which pt_regs the user-level registers are in
	 * is necessary.
	 */
}

static inline struct pt_regs* get_user_regs_thread(struct task_struct *thd)
{
	return (struct pt_regs*)((int)thd->thread.sp0 - sizeof(struct pt_regs));
}

/* copy the current user-space regs to user-level */
static inline int write_regs_to_user(struct pt_regs * __user user_regs_area)
{
	struct pt_regs *regs = get_user_regs();

	if (copy_to_user(user_regs_area, regs, sizeof(struct pt_regs))){
		//printk("cosWriting registers to user-level failed.\n");
		return -EFAULT;
	}

	return 0;
}

/* copy regs from user to a kernel location. */
/* FIXME: inconsistent order of args with memcpy and copy_from_user */
static inline int save_user_regs_to_kern(struct pt_regs * __user user_regs_area, struct pt_regs *kern_regs)
{
	if (copy_from_user(kern_regs, user_regs_area, sizeof(struct pt_regs))) {
		//printk("cos");
		return -EFAULT;
	}

	return 0;
}

/* not necessarily compiled in */
#ifndef HPAGE_SHIFT
#define HPAGE_SHIFT	22
#define HPAGE_SIZE	((1UL) << HPAGE_SHIFT)
#define HPAGE_MASK	(~(HPAGE_SIZE - 1))
#endif
#define PTE_MASK PAGE_MASK

/*
 * Find the corresponding pte for the address and return its virtual
 * address.  Mainly a debugging tool to see if we are setting up the
 * correct page mappings, but also used to set characteristics in all
 * pages.
 */
static inline pte_t *lookup_address_mm(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd = pgd_offset(mm, addr);
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

/* FIXME: change to clone_pgd_range */
static inline void copy_pgd_range(struct mm_struct *to_mm, struct mm_struct *from_mm,
				  unsigned long lower_addr, unsigned long size)
{
	pgd_t *tpgd = pgd_offset(to_mm, lower_addr);
	pgd_t *fpgd = pgd_offset(from_mm, lower_addr);
	unsigned int span = hpage_index(size);

#ifdef NIL
	if (!(pgd_val(*fpgd) & PGTBL_PRESENT)) {
		printk("cos: BUG: nothing to copy in mm %p's pgd @ %x.\n",
		       from_mm, (unsigned int)lower_addr);
	}
#endif

	/* sizeof(pgd entry) is intended */
	memcpy(tpgd, fpgd, span*sizeof(unsigned int));
}

#define flush_all(pgdir) load_cr3(pgdir)
#define my_load_cr3(pgdir) asm volatile("movl %0,%%cr3": :"r" (__pa(pgdir)))
#define flush_executive(pgdir) my_load_cr3(pgdir)

struct spd_poly linux_pgtbls_per_core[NUM_CPU];

/* Create a boot thread. Used by ioctl only. */
struct thread *ready_boot_thread(struct spd *init)
{
//	struct shared_user_data *ud = get_shared_data();
	struct thread *thd;
	unsigned int tid;
	struct spd_poly *this_pgtbl;
	struct thd_invocation_frame *frame;

	assert(NULL != init);

	thd = thd_alloc(init);
	if (NULL == thd) {
		printk("cos: Could not allocate boot thread.\n");
		return NULL;
	}
	/*
	 * Create the spd_poly with a pointer to the page tables for
	 * each Linux process to return to, so that when the separate
	 * core's cos threads return to comp0 (thus the cos_loader and
	 * Linux in general), we will return to the _separate_ and
	 * correct page-tables.
	 */
	printk("Setting up boot thread on core %d\n", get_cpuid());
	this_pgtbl                   = &linux_pgtbls_per_core[get_cpuid()];
	assert(this_pgtbl);
	this_pgtbl->pg_tbl           = (paddr_t)(__pa(current->mm->pgd));
	cos_ref_set(&this_pgtbl->ref_cnt, 2);
	frame                        = thd_invstk_top(thd);
	assert(thd->stack_ptr == 0);
	frame->current_composite_spd = this_pgtbl;

	assert(init->location[0].lowest_addr == SERVICE_START);
	assert(thd_spd_in_composite(this_pgtbl, init));

	tid = thd_get_id(thd);
	cos_put_curr_thd(thd);

	assert(tid);

//	switch_thread_data_page(2, tid);
	/* thread ids start @ 1 */
//	ud->current_thread = tid;
//	ud->argument_region = (void*)((tid * PAGE_SIZE) + COS_INFO_REGION_ADDR);

	return thd;
}

static int syscalls_enabled = 1;

extern int virtual_namespace_alloc(struct spd *spd, unsigned long addr, unsigned int size);

/* We need to save cos thread for each core. This is used when switch host pg tables.*/
void save_per_core_cos_thd(void)
{
        cos_thd_per_core[get_cpuid()].cos_thd = current;

        return;
}

static inline void hw_int_override_all(void)
{
	load_per_core_TSS();
	hw_int_override_sysenter(sysenter_interposition_entry, (void *)get_cpu_var(x86_tss));
	hw_int_override_pagefault(page_fault_interposition);
	hw_int_override_idt(0, div_fault_interposition, 0, 0);
	hw_int_override_idt(COS_REG_SAVE_VECTOR, reg_save_interposition, 0, 3);
#ifdef FPU_ENABLED
        hw_int_override_idt(7, fpu_not_available_interposition, 0, 0);
#endif
	hw_int_cos_ipi(ipi_handler);
	hw_int_override_timer(timer_interposition);

	return;
}

static void hw_reset(void *data)
{
	load_per_core_TSS();
	hw_int_reset(get_cpu_var(x86_tss));
}

#define THD_SIZE (PAGE_SIZE/4)

u8_t init_thds[THD_SIZE * NUM_CPU] PAGE_ALIGNED;
/* Reserve 5 pages for llboot captbl. */
u8_t boot_comp_captbl[PAGE_SIZE*BOOT_CAPTBL_NPAGES] PAGE_ALIGNED;
u8_t c0_comp_captbl[PAGE_SIZE] PAGE_ALIGNED;

u8_t *boot_comp_pgd;
u8_t *boot_comp_pte_vm;
u8_t *boot_comp_pte_km;//[COS_KERNEL_MEMORY / (PAGE_SIZE/sizeof(void *))];
u8_t *boot_comp_pte_pm;

#define N_PHYMEM_PAGES COS_MAX_MEMORY /* # of physical pages available */

static void *cos_kmem, *cos_kmem_base;
void *linux_pgd;
vaddr_t boot_sinv_entry;
unsigned long sys_llbooter_sz;    /* how many pages is the llbooter? */
void *llbooter_kern_mapping;

struct thread *__thd_current;

struct captbl *boot_captbl;

/* We need global thread name space as we use thd_id to access simple
 * stack. When we have low-level per comp stack free-list, we don't
 * have to use global thread id name space.*/
u32_t free_thd_id = 1;

static void *
get_coskmem(void)
{
	void *ret = cos_kmem;
	cos_kmem += PAGE_SIZE;

	return ret;
}

//#define KMEM_HACK
#ifdef KMEM_HACK
/* a hack to get more kmem... */
#define NBOOTKMEM 1024
void *bootkmem[NBOOTKMEM];
unsigned long bootkmem_used = 0;
#endif

static int
kern_boot_comp(struct spd_info *spd_info)
{
	int ret;
	struct captbl *ct, *ct0;
	pgtbl_t pt, pt0;
	unsigned int i, n_pte_entry, n_pte;
	void *kmem_base_pa;
	struct cap_pgtbl *pte_cap;

	ct = captbl_create(boot_comp_captbl);
	assert(ct);

	/* expand the captbl to use multiple pages. */
	for (i = 1; i < BOOT_CAPTBL_NPAGES; i++) {
		captbl_init(boot_comp_captbl + i * PAGE_SIZE, 1);
		ret = captbl_expand(ct, (PAGE_SIZE*i - PAGE_SIZE/2)/CAPTBL_LEAFSZ, captbl_maxdepth(), boot_comp_captbl + PAGE_SIZE*i);
		assert(!ret);
		captbl_init(boot_comp_captbl + PAGE_SIZE + PAGE_SIZE/2, 1);
		ret = captbl_expand(ct, (PAGE_SIZE*i)/CAPTBL_LEAFSZ, captbl_maxdepth(), boot_comp_captbl + PAGE_SIZE*i + PAGE_SIZE/2);
		assert(!ret);
	}
	boot_captbl = ct;

	kmem_base_pa = (void *)chal_va2pa(cos_kmem_base);
	ret = retypetbl_retype2kern(kmem_base_pa);
	assert(ret == 0);

	boot_comp_pgd = get_coskmem();
	boot_comp_pte_km = get_coskmem();

	pt = pgtbl_create(boot_comp_pgd, (void *)chal_va2pa(linux_pgd));
	assert(pt);
	pgtbl_init_pte(boot_comp_pte_km);

	if (captbl_activate_boot(ct, BOOT_CAPTBL_SELF_CT)) cos_throw(err, -1);
	if (sret_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SRET)) cos_throw(err, -1);
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT, pt, 0)) cos_throw(err, -1);

	/* KMEM PTE */
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_KM_PTE, (pgtbl_t)(boot_comp_pte_km), 1)) cos_throw(err, -1);
	if (cap_cons(ct, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_KM_PTE, BOOT_MEM_KM_BASE)) cos_throw(err, -1);

	/* VM PTEs */
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_BOOTVM_PTE, NULL, 1)) cos_throw(err, -1);
	/* Get the cap, and we'll set the pgtbl next. We don't usually
	 * do this, just for booting up. */
	pte_cap = (struct cap_pgtbl *)captbl_lkup(ct, BOOT_CAPTBL_BOOTVM_PTE);
	assert(pte_cap);
	n_pte_entry = PAGE_SIZE / sizeof(void *);

	for (i = 0; i < BOOTER_NREGIONS; i++) {
		boot_comp_pte_vm = get_coskmem();
		if (((u32_t)boot_comp_pte_vm - (u32_t)cos_kmem_base) % RETYPE_MEM_SIZE == 0) {
			ret = retypetbl_retype2kern((void *)chal_va2pa(boot_comp_pte_vm));
			assert(ret == 0);
		}

		pgtbl_init_pte(boot_comp_pte_vm);
		pte_cap->pgtbl = (pgtbl_t)boot_comp_pte_vm;

		/* construct the page tables */
		if (cap_cons(ct, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_BOOTVM_PTE,
			     BOOT_MEM_VM_BASE + i*n_pte_entry*PAGE_SIZE)) { printk("failed i %d\n", i);cos_throw(err, -1);}
	}

	/* PM PTEs */
	n_pte = COS_MAX_MEMORY / n_pte_entry;
	if (COS_MAX_MEMORY % n_pte_entry) n_pte++;

	ret = pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_PHYSM_PTE, NULL, 1);
	pte_cap = (struct cap_pgtbl *)captbl_lkup(ct, BOOT_CAPTBL_PHYSM_PTE);
	assert(pte_cap);

#ifdef KMEM_HACK
	if (n_pte > NBOOTKMEM) {
		printk("no enough bootkmem, want %d\n", n_pte);
		cos_throw(err, -1);
	}
	for (i = 0; i < NBOOTKMEM; i++)
		bootkmem[i] = NULL;

	/* Get PTE */
	for (i = 0; i < n_pte; i++) {
		bootkmem[i] = chal_alloc_page();
		if (!bootkmem[i]) {
			printk("no bootkmem\n");
			cos_throw(err, -1);
		}
	}
	bootkmem_used += n_pte;
#endif

	for (i = 0; i < n_pte; i++) {
#ifndef KMEM_HACK
		boot_comp_pte_pm = get_coskmem();
		if (((u32_t)boot_comp_pte_pm - (u32_t)cos_kmem_base) % RETYPE_MEM_SIZE == 0) {
			ret = retypetbl_retype2kern((void *)chal_va2pa(boot_comp_pte_pm));
			assert(ret == 0);
		}
#else
		boot_comp_pte_pm = bootkmem[i];
#endif

		pgtbl_init_pte(boot_comp_pte_pm);
		/* Again, a hack for bootstrap. */
		pte_cap->pgtbl = (pgtbl_t)boot_comp_pte_pm;

		if (cap_cons(ct, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_PHYSM_PTE,
			     BOOT_MEM_PM_BASE + i*n_pte_entry*PAGE_SIZE)) cos_throw(err, -1);
	}

	sys_llbooter_sz = spd_info->mem_size / PAGE_SIZE;
	if (spd_info->mem_size % PAGE_SIZE) sys_llbooter_sz++;

#ifdef KMEM_HACK
	for (i = 0; i < sys_llbooter_sz; i++) {
		bootkmem[i + bootkmem_used] = chal_alloc_page();
		if (!bootkmem[i+bootkmem_used]) {
			printk("no bootkmem\n");
			cos_throw(err, -1);
		}
	}
#endif
	/* add the component's virtual memory at 4MB (1<<22) using "physical memory" starting at cos_kmem */
	for (i = 0 ; i < sys_llbooter_sz; i++) {
#ifndef KMEM_HACK
		u32_t flags;
		u32_t addr = (u32_t)((void *)chal_va2pa(cos_kmem) + i*PAGE_SIZE);
		if ((addr - (u32_t)kmem_base_pa) % RETYPE_MEM_SIZE == 0) {
			ret = retypetbl_retype2kern((void *)addr);
			if (ret) {
				printk("Retype paddr %x failed when loading llbooter. ret %d\n", addr, ret);
 				cos_throw(err, -1);
			}
		}
		if (pgtbl_mapping_add(pt, BOOT_MEM_VM_BASE + i*PAGE_SIZE, addr, PGTBL_USER_DEF)) {
			printk("Mapping llbooter %x failed!\n", addr);
			cos_throw(err, -1);
		}
		assert(chal_pa2va((paddr_t)addr) == pgtbl_lkup(pt, BOOT_MEM_VM_BASE+i*PAGE_SIZE, &flags));
#else
		u32_t addr = (u32_t)chal_va2pa(bootkmem[i+bootkmem_used]);
		kmem_add_hack(pt, BOOT_MEM_VM_BASE + i*PAGE_SIZE, addr, PGTBL_USER_DEF);
#endif
	}

#ifndef KMEM_HACK
	llbooter_kern_mapping = cos_kmem;
	cos_kmem += sys_llbooter_sz*PAGE_SIZE;
#else
//	llbooter_kern_mapping = bootkmem[bootkmem_used];
	bootkmem_used += sys_llbooter_sz;
#endif

	/* Round to the next memory retype region. Adjust based on
	 * offset from cos_kmem_base*/
	if ((cos_kmem - cos_kmem_base) % RETYPE_MEM_SIZE != 0) {
		cos_kmem += (RETYPE_MEM_SIZE - (cos_kmem - cos_kmem_base) % RETYPE_MEM_SIZE);
	}

	/* add the remaining kernel memory @ 1.5GB*/
	/* printk("mapping from kmem %x\n", cos_kmem); */
	for (i = 0; i < (COS_KERNEL_MEMORY - (cos_kmem - cos_kmem_base)/PAGE_SIZE); i++) {
		u32_t addr = (u32_t)((void *)chal_va2pa(cos_kmem) + i*PAGE_SIZE);

		if (pgtbl_cosframe_add(pt, BOOT_MEM_KM_BASE + i*PAGE_SIZE,
				       addr, PGTBL_COSFRAME)) cos_throw(err, -1);
	}
	printk("cos: %d kernel accessible pages @ %x\n", i, BOOT_MEM_KM_BASE);

	if (COS_MEM_START % RETYPE_MEM_SIZE != 0) {
		printk("Physical memory start address (%d) not aligned by retype_memory size (%lu).",
		       COS_MEM_START, RETYPE_MEM_SIZE);
		cos_throw(err, -1);
	}

	/* add the system's physical memory at address 2GB */
	for (i = 0 ; i < N_PHYMEM_PAGES ; i++) {
		u32_t addr = COS_MEM_START + i*PAGE_SIZE;

		if (pgtbl_cosframe_add(pt, BOOT_MEM_PM_BASE + i*PAGE_SIZE,
				       addr, PGTBL_COSFRAME)) { printk ("%d failed\n", i);break;}//cos_throw(err, -1);
	}
	printk("cos: %d user frames @ %x\n", i, BOOT_MEM_PM_BASE);

	/* comp0's data, culminated in a static invocation capability to the llbooter */
	ct0 = captbl_create(c0_comp_captbl);
	assert(ct0);
	if (captbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_COMP0_CT, ct0, 0)) cos_throw(err, -1);
	pt0 = (void *)chal_va2pa(current->mm->pgd);
	assert(pt0);
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_COMP0_PT, pt0, 0)) cos_throw(err, -1);

	assert(spd_info->upcall_entry);
	if (comp_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT, 0, spd_info->upcall_entry, NULL)) cos_throw(err, -1);
	if (comp_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_COMP0_COMP,
			  BOOT_CAPTBL_COMP0_CT, BOOT_CAPTBL_COMP0_PT, 0, 0, NULL)) cos_throw(err, -1);
	/*
	 * Only capability for the comp0 is 4: the synchronous
	 * invocation capability.
	 */
	assert(boot_sinv_entry);

	if (sinv_activate(ct, BOOT_CAPTBL_COMP0_CT, 4, BOOT_CAPTBL_SELF_COMP, boot_sinv_entry)) cos_throw(err, -1);

	return 0;
err:
	printk("Activating data-structure failed.\n");
	return ret;
}

static long aed_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch(cmd) {
	case AED_INIT_BOOT_THD:
	{
		u64_t s,e;
		struct thread *thd = (struct thread *)(init_thds + get_cpuid()*THD_SIZE);
		assert(get_cpuid() < NUM_CPU_COS);

		cos_kern_stk_init();
		save_per_core_cos_thd();

		/*
		 * Create a thread in comp0.
		 */
		rdtscll(s);
		while (thd_activate(boot_captbl, BOOT_CAPTBL_SELF_CT,
				    BOOT_CAPTBL_SELF_INITTHD_BASE + get_cpuid() * captbl_idsize(CAP_THD),
				    thd, BOOT_CAPTBL_COMP0_COMP, 0)) {
			/* CAS could fail on init. */
			rdtscll(e);
			if ((e-s) > (1<<30)) return -EFAULT;
		}

		thd_current_update(thd, thd, cos_cpu_local_info());

		/* Comp0 has only 1 pgtbl, which points to the process
		 * of the init core. Here we update the inv_stk of the
		 * init thread of current core. */
		thd->invstk[0].comp_info.pgtbl = (void *)chal_va2pa(current->mm->pgd);

		hw_int_override_all();

		return 0;
	}
	case AED_INIT_BOOT:
	{
		struct spd_info spd_info;

		if (copy_from_user(&spd_info, (void*)arg,
				   sizeof(struct spd_info))) {
			printk("cos: Error copying spd_info from user.\n");
			return -EFAULT;
		}
		linux_pgd = current->mm->pgd;

		cap_init();
		ltbl_init();
		retype_tbl_init();
		comp_init();
		thd_init();
		inv_init();

		if (kern_boot_comp(&spd_info)) return -1;

#ifndef KMEM_HACK
		assert(llbooter_kern_mapping);
		if (copy_from_user(llbooter_kern_mapping, (void*)spd_info.lowest_addr, spd_info.mem_size)) {
			printk("cos: Error copying spd_info from user.\n");
			return -EFAULT;
		}
#else
		int i;
		for (i = 0; i < sys_llbooter_sz-1; i++) {
			if (copy_from_user(bootkmem[i+bootkmem_used-sys_llbooter_sz], (void*)(spd_info.lowest_addr+i*PAGE_SIZE), PAGE_SIZE)) {
				printk("cos: Error copying spd_info from user.\n");
				return -EFAULT;
			}
		}
		copy_from_user(bootkmem[bootkmem_used-1], (void*)(spd_info.lowest_addr+i*PAGE_SIZE), spd_info.mem_size - i*PAGE_SIZE);
#endif
		return 0;
	}
	case AED_CREATE_SPD:
	{
		struct spd_info spd_info;
		struct spd *spd;
		int i;

		if (copy_from_user(&spd_info, (void*)arg,
				   sizeof(struct spd_info))) {
			printk("cos: Error copying spd_info from user.\n");
			return -EFAULT;
		}

		if (!spd_info.ucap_tbl ||
		    !access_ok(VERIFY_WRITE, spd_info.ucap_tbl, spd_info.num_caps)) {
			return -EINVAL;
		}

		spd = spd_alloc(spd_info.num_caps,
				(struct usr_inv_cap *)spd_info.ucap_tbl,
				spd_info.upcall_entry);

		if (!spd) {
			printk("cos: Could not allocate spd.\n");
			return -ENOMEM;
		}

		for (i = 0 ; i < COS_NUM_ATOMIC_SECTIONS ; i++) {
			spd->atomic_sections[i] = spd_info.atomic_regions[i];
		}

		/* This is a special case where the component should
		 * share the address space of the configuring process,
		 * thus have access to all component's memory.  This
		 * won't survive iterations, but for now it is a
		 * convenient mechanism so that I don't have to setup
		 * stubs and ipc to the configuration process
		 * itself. */
		if (spd_info.lowest_addr == 0) {
			spd->spd_info.pg_tbl         = (paddr_t)(__pa(current->mm->pgd));
			spd->location[0].lowest_addr = SERVICE_START;
			spd->location[0].size        = PGD_RANGE;
			spd->composite_spd           = &spd->spd_info;
		} else {
			/*
			 * Copy relevant page table entries from the
			 * master page tables into this component's
			 * tables.  This includes not only the
			 * component's memory entries, but also the
			 * shared region.
			 */
			struct mm_struct *mm;
			struct composite_spd *cspd;

			spd->local_mmaps = aed_allocate_mm();
			if (spd->local_mmaps < 0) {
				spd_free(spd);
				return spd->local_mmaps;
			}
			mm = aed_get_mm(spd->local_mmaps);

			if (!virtual_namespace_alloc(spd, spd_info.lowest_addr, spd_info.size)) {
				printk("cos: collision in virtual namespace.\n");
				aed_free_mm(spd->local_mmaps);
				spd_free(spd);
				return -1;
			}

			copy_pgd_range(mm, current->mm, spd_info.lowest_addr, spd_info.size);

			cspd = spd_alloc_mpd();
			if (!cspd) {
				printk("cos: Could not allocate composite spd for initial spd.\n");
				aed_free_mm(spd->local_mmaps);
				spd_free(spd);
				return -1;
			}

			spd_set_location(spd, spd_info.lowest_addr, spd_info.size, (paddr_t)(__pa(mm->pgd)));

			if (spd_composite_add_member(cspd, spd)) {
				printk("cos: could not add spd %d to composite spd %d.\n",
				       spd_get_index(spd), spd_mpd_index(cspd));
				return -1;
			}

			spd->pfn_base        = 0;
			spd->pfn_extent      = COS_MAX_MEMORY;
			spd->kern_pfn_base   = 0;
			spd->kern_pfn_extent = COS_KERNEL_MEMORY;
		}

		return spd_get_index(spd);
	}
	case AED_SPD_ADD_CAP:
	{
		struct cap_info cap_info;
		struct spd *owner, *dest;
		int cap_no;

		if (copy_from_user(&cap_info, (void*)arg,
				   sizeof(struct cap_info))) {
			printk("cos: Error copying cap_info from user.\n");
			return -EFAULT;
		}

		owner = spd_get_by_index(cap_info.owner_spd_handle);
		if (!owner) {
			printk("cos: could not find owner to create cap.\n");
			return -EINVAL;
		}

		dest  = spd_get_by_index(cap_info.dest_spd_handle);
		if (!dest) {
			printk("cos: could not find dest to create cap.\n");
			return -EINVAL;
		}

		cap_no = spd_add_static_cap_extended(owner, dest, cap_info.rel_offset,
						     cap_info.ST_serv_entry, cap_info.AT_cli_stub, cap_info.AT_serv_stub,
						     cap_info.SD_cli_stub, cap_info.SD_serv_stub, cap_info.il, cap_info.flags);

		/* FIXME: we need to get sched_init entry of llboot. */
		if (spd_get_index(owner) == 0 && spd_get_index(dest) == 1 && cap_no == 1) {
			//printk("cap %d, boot addr stub %x\n", cap_no, cap_info.SD_serv_stub);
			// cap 2 is fault_handler.
			boot_sinv_entry = cap_info.SD_serv_stub;
		}

		return cap_no;
	}
	case AED_CREATE_THD:
	{
		struct cos_thread_info thread_info;
		struct thd_sched_info *tsi;
		int i;
		struct thread *thd;
		struct spd *spd, *sched;

		cos_kern_stk_init();

		save_per_core_cos_thd();

		if (copy_from_user(&thread_info, (void*)arg,
				   sizeof(struct cos_thread_info))) {
			//printk("cos: Error copying thread_info from user.\n");
			return -EFAULT;
		}

		/* printk("COS AED IOCTL: core %d creating thread in spd %d.\n",  */
		/*        get_cpuid(), thread_info.spd_handle); */
		if (fpu_init() < 0) {
			printk("cos: FPU init failed.\n");
			return -EFAULT;
		}

		/* printk("cos core %u: creating thread in spd %d.\n", get_cpuid(), thread_info.spd_handle); */

		/* spd = spd_get_by_index(thread_info.spd_handle); */
		/* if (!spd) { */
		/* 	printk("cos: Spd %d invalid for thread creation.\n",  */
		/* 	       thread_info.spd_handle); */
		/* 	return -EINVAL; */
		/* } */
		/* thd = ready_boot_thread(spd); */
		/* spd = spd_get_by_index(thread_info.sched_handle); */
		/* if (!spd) { */
		/* 	printk("cos: scheduling spd %d not permitted to create thread.\n",  */
		/* 	       thread_info.sched_handle); */
		/* 	thd_free(thd); */
		/* 	return -EINVAL; */
		/* } */
		/* sched = spd; */
		/* for (i = spd->sched_depth ; i >= 0 ; i--) { */
		/* 	tsi = thd_get_sched_info(thd, i); */
		/* 	tsi->scheduler = sched; */
		/* 	sched = sched->parent_sched; */
		/* } */

		/* FIXME: need to return opaque handle, rather than
		 * just set the current thread to be the new one. */

		return 0;
	}
	case AED_CAP_CHANGE_ISOLATION:
	{
		isolation_level_t prev_lvl;
		struct cap_info cap_info;
		struct spd *spd;

		if (copy_from_user(&cap_info, (void*)arg,
				   sizeof(struct cap_info))) {
			//printk("cos: Error copying cap_info from user.\n");
			return -EFAULT;
		}

		spd = spd_get_by_index(cap_info.owner_spd_handle);

		if (cap_is_free(spd, cap_info.cap_handle)) {
			return -EINVAL;
		}

		if (cap_info.il > MAX_ISOLATION_LVL_VAL) {
			return -EINVAL;
		}

		prev_lvl = cap_change_isolation(spd, cap_info.cap_handle, cap_info.il, cap_info.flags);

		return prev_lvl;
	}
	case AED_PROMOTE_SCHED:
	{
		struct spd_sched_info sched_info;
		struct spd *sched;

		if (copy_from_user(&sched_info, (void*)arg,
				   sizeof(struct spd_sched_info))) {
			printk("cos: Error copying spd scheduler info from user-level.\n");
			return -EFAULT;
		}

		if (sched_info.spd_sched_handle > MAX_NUM_SPDS ||
		    sched_info.spd_sched_handle < 0 ||
		    sched_info.spd_parent_handle > MAX_NUM_SPDS) {
			printk("cos: spd handle out of range %d and %d.\n",
			       sched_info.spd_sched_handle, sched_info.spd_parent_handle);
			return -EINVAL;
		}

		if (spd_is_free(sched_info.spd_sched_handle) ||
		    (sched_info.spd_parent_handle >= 0 &&
		     spd_is_free(sched_info.spd_parent_handle))) {
			printk("cos: spd is already free %d and %d.\n",
			       sched_info.spd_sched_handle, sched_info.spd_parent_handle);
			return -EINVAL;
		}

		sched = spd_get_by_index(sched_info.spd_sched_handle);
		if (sched_info.spd_parent_handle < 0) {
			sched->sched_depth = 0;
		} else {
			struct spd *p = spd_get_by_index(sched_info.spd_parent_handle);
			int parent_sdepth = p->sched_depth;

			if (parent_sdepth == MAX_SCHED_HIER_DEPTH || parent_sdepth == -1) {
				printk("cos: max scheduler hierarchy depth over- or under-run.\n");
				return -EINVAL;
			}

			sched->parent_sched = p;
			sched->sched_depth = p->sched_depth + 1;
		}

		return 0;
	}
	case AED_EMULATE_PREEMPT:
	{
		struct pt_regs *regs = get_user_regs_thread(cos_thd_per_core[get_cpuid()].cos_thd);
		struct thread *cos_thd = cos_get_curr_thd();
		//struct pt_regs *irq_regs = get_irq_regs();

		memcpy(&cos_thd->regs, regs, sizeof(struct pt_regs));

		/* ... skipped this and went for the real thing with
		 * the timer interrupt */

		return 0;
	}
	case AED_DISABLE_SYSCALLS:
		syscalls_enabled = 0;
		return 0;
	case AED_ENABLE_SYSCALLS:
		syscalls_enabled = 1;
		return 0;
	case AED_RESTORE_HW_ENTRY:
	{
		struct cos_cpu_local_info *cos_info;

		cos_info = cos_cpu_local_info();
		if (cos_info->overflow_check != 0xDEADBEEF) {
			/* Should never happen. */
			printk("Warning: kernel stack overflow detected (detector %x)!\n", (unsigned int)cos_info->overflow_check);
		}
		hw_reset(NULL);

		return 0;
	}
	default:
		ret = -EINVAL;
	}

	return ret;
}

#define NFAULTS 200
int fault_ptr = 0;
struct fault_info {
	vaddr_t addr;
	int err_code;
	struct pt_regs regs;
	unsigned short int spdid, thdid;
	int cspd_flags, cspd_master_flags;
	unsigned long long timestamp;
} faults[NFAULTS];

static void cos_report_fault(struct thread *t, vaddr_t fault_addr, int ecode, struct pt_regs *regs)
{
	struct fault_info *fi;
	unsigned long long ts;
	struct spd_poly *spd_poly;

	if (fault_ptr >= NFAULTS-1) return;

	rdtscll(ts);

	fi = &faults[fault_ptr];
	fi->addr = fault_addr;
	fi->err_code = ecode;
	if (NULL != regs) memcpy(&fi->regs, regs, sizeof(struct pt_regs));
	fi->spdid = spd_get_index(thd_get_thd_spd(t));
	fi->thdid = thd_get_id(t);
	fi->timestamp = ts;
	spd_poly = thd_invstk_top(t)->current_composite_spd;
	fi->cspd_flags = spd_poly->flags;
	if (spd_poly->flags & SPD_SUBORDINATE) {
		struct composite_spd *cspd = ((struct composite_spd *)spd_poly)->master_spd;
		fi->cspd_master_flags = cspd->spd_info.flags;
	} else {
		fi->cspd_master_flags = 0;
	}
	fault_ptr = (fault_ptr + 1) % NFAULTS;
}

#define SHARED_DATA_PAGE_SIZE PAGE_SIZE
extern unsigned int shared_region_page[1024], shared_data_page[1024];

/*
 * FIXME: this logic should be in inv.c or platform independent code
 *
 * Before we look for the Linux vma, lets check if we should look at
 * all, or if Composite can fix up the fault on its own.
 */
static int
cos_prelinux_handle_page_fault(struct thread *thd, struct pt_regs *regs,
			       vaddr_t fault_addr)
{
	struct spd_poly *active = thd_get_thd_spdpoly(thd), *curr;
	struct composite_spd *cspd;
	vaddr_t ucap_addr = regs->ax;
	struct spd *origin;
	struct pt_regs *regs_save;

	/* FIXME: the Composite path doesn't work right now. This
	 * still accesses spd struct. */

	/*
	 * If we are in the most up-to-date version of the
	 * page-tables, then there is no fixing up to do, and we
	 * should just return.  Check for this case.
	 */
	if (!active) return 0;
	cspd = (struct composite_spd *)active;
	assert(cspd);
	if (!(spd_mpd_is_depricated(cspd) ||
	      (spd_mpd_is_subordinate(cspd) &&
	       spd_mpd_is_depricated(cspd->master_spd)))) return 0;
	assert(active->flags & SPD_COMPOSITE);

	/*
	 * We are going to perform these checks in order:
	 *
	 * Assume: regs->eax (thus ucap_addr) contains the address of
	 * the user-level capability structure.
	 *
	 * 1) lookup the origin of the invocation (via the user-cap)
	 *
	 * 2) verify that the user-capability is valid (within bounds
         * allocated to the active protection domain).
	 *
	 * 3) verify that the faulted pud is not in the active pd
	 *
	 * 4) check that it is present in the current pd config.
	 *
	 * 5) map that entry into the active page tables.
	 */

	/* 1 */
	origin = NULL;
	if (unlikely(NULL == origin)) return 0;
	/* up-to-date pd */
	curr = origin->composite_spd;

	/* 2 */
	if (unlikely(chal_pgtbl_entry_absent(active->pg_tbl, ucap_addr))) return 0;

	/* 3: really don't know what could cause this */
	if (unlikely(!chal_pgtbl_entry_absent(active->pg_tbl, fault_addr))) return 0;

	/* 4 */
	if (unlikely(chal_pgtbl_entry_absent(curr->pg_tbl, fault_addr))) return 0;

	/* 5
	 *
	 * Extend the current protection domain to include mappings of
	 * a more up-to-date pd if this one is subordinate and not
	 * consistent.
	 */
	chal_pgtbl_copy_range(active->pg_tbl, curr->pg_tbl, fault_addr, HPAGE_SIZE);

	/*
	 * NOTE: perhaps a better way to do this would be to look up
	 * the spds associated with both addresses (ucap, and fault),
	 * and check to make sure that their ->composite_spd is the
	 * same, and if so, map the entry into the active page table.
	 * We'll still need to make sure that the ucap is active in
	 * the current page table though, so I don't know if we save
	 * anything.
	 */

	regs_save = &((struct pt_regs*)(((char*)shared_data_page)+SHARED_DATA_PAGE_SIZE))[-1];
	memcpy(regs_save, regs, sizeof(struct pt_regs));

	return 1;
}

static void
cos_record_fault_regs(struct thread *t, vaddr_t fault_addr, int ecode, struct pt_regs *rs)
{
	memcpy(&t->regs, rs, sizeof(struct pt_regs));
	cos_report_fault(t, fault_addr, ecode, rs);
}

extern int fault_update_mpd_pgtbl(struct thread *thd, struct pt_regs *regs, vaddr_t fault_addr);
extern void
fault_ipc_invoke(struct thread *thd, vaddr_t fault_addr, int flags, struct pt_regs *regs, int fault_num);

/* the composite specific page fault handler */
static int
cos_handle_page_fault(struct thread *thd, vaddr_t fault_addr,
		      int ecode, struct pt_regs *regs)
{
	cos_record_fault_regs(thd, fault_addr, ecode, regs);
	fault_ipc_invoke(thd, fault_addr, 0, regs, COS_FLT_PGFLT);

	return 0;
}


//#define FAULT_DEBUG
/*
 * Called from page_fault_interposition in kern_entry.S.  Interrupts
 * disabled...dont block!  Can assume current_active_guest is accessed
 * in a critical section due to this.
 *
 * COS - things to check:
 * 1) are we faulting in the shared region
 * 2) are we faulting in a component for which linux defines a mapping
 * 3) are we faulting in a component which has no mapping
 * 4) if we are legally faulting on a function call to a server that
 *    used to be isolated via ST, but is now SDT (thus the page fault)
 *
 * #3 is the most complicated as we must save the registers into a
 * fault region provided by another process, and execute the fault
 * handler.
 */
#define BUCKET_ORDER 10
#define BUCKET_MASK (~((~(0UL))>>BUCKET_ORDER))
#define BUCKET_HASH(x) ((BUCKET_MASK & (x)) >> (32-BUCKET_ORDER))
#define NUM_BUCKETS (1UL<<BUCKET_ORDER)
#ifdef FAULT_DEBUG
static unsigned long fault_addrs[NUM_BUCKETS];
#endif

/* checks on the error code provided for x86 page faults */
#define PF_PERM(code)   (code & 0x1)
#define PF_ABSENT(code) (!PF_PERM(code))
#define PF_USER(code)   (code & 0x4)
#define PF_KERN(code)   (!PF_USER(code))
#define PF_WRITE(code)  (code & 0x2)
#define PF_READ(code)   (!PF_WRITE(code))

void hijack_syscall_monitor(int num)
{
	if (unlikely(!syscalls_enabled && cos_thd_per_core[get_cpuid()].cos_thd == current)) {
		printk("Warning: making a Linux system call (#%d) in Composite. Ignore once.\n", num);
	}
}

/*
 * This function will be called upon a hardware page fault.  Return 0
 * if you want the linux page fault to be run, !0 otherwise.
 */
__attribute__((regparm(3)))
int main_page_fault_interposition(struct pt_regs *rs, unsigned int error_code)
{
	struct vm_area_struct *vma;
	struct mm_struct *curr_mm;

	unsigned long fault_addr;
	struct thread *thd;
	int ret = 1;

	fault_addr = read_cr2();

	if (fault_addr > KERN_BASE_ADDR) goto linux_handler;

	/*
	 * Composite doesn't know how to handle kernel faults, and
	 * they should be sent by the assembly to the default linux
	 * handler.
	 */
	assert(!PF_KERN(error_code));

	/*
	 * We want to allow composite to handle the fault if we are in
	 * the composite thread and either the fault was outside the
	 * spd's boundaries or there is not a linux mapping for the
	 * address.
	 */

	if (cos_thd_per_core[get_cpuid()].cos_thd != current) goto linux_handler;
	if (fault_addr == (unsigned long)&page_fault_interposition) goto linux_handler;
	curr_mm = get_task_mm(current);

	if (!down_read_trylock(&curr_mm->mmap_sem)) {
		/*
		 * The semaphore can only be taken while code is
		 * executing in the kernel.  If a page fault occurs in
		 * the kernel, then we shouldn't be dealing with it in
		 * Composite as it is due to either 1) an error: let
		 * Linux send the kill signal, or 2) a fault that will
		 * be resolved by the exception tables.
		 */
		goto linux_handler_put;
	}

	/*
	 * The composite current thread will only be null if we have
	 * not completely initialized composite yet, but we need to
	 * check for this.
	 */
	thd = cos_get_curr_thd();
	/* This is a magical address that we are getting faults for,
	 * but I don't know why, and it doesn't seem to interfere with
	 * execution.  For now ffffd0b0 is being counted as an unknown
	 * fault so that it won't get reported as a cos fault where we
	 * can do nothing about it.
	 *
	 * OK, I think I understand now.  That is the virtual syscall
	 * page.  It is faulted in when it is accessed after a
	 * time-slice so that it can be updated to reflect the current
	 * get_time_of_day, and from then on, it is accessed directly.
	 */
	if (NULL == thd || fault_addr == 0xffffd0b0 || fault_addr == 0xfffffffa) {
		cos_meas_event(COS_UNKNOWN_FAULT);
		goto linux_handler_release;
	}
#ifdef FAULT_DEBUG
	fault_addrs[BUCKET_HASH(fault_addr)]++;
#endif
	if (PF_ABSENT(error_code) && PF_READ(error_code) &&
	    cos_prelinux_handle_page_fault(thd, rs, fault_addr)) {
		ret = 0;
		goto linux_handler_release;
	}
	vma = find_vma(curr_mm, fault_addr);
	if (vma && vma->vm_start <= fault_addr) {
		/* let the linux fault handler deal with it */
		cos_meas_event(COS_LINUX_PG_FAULT);
		goto linux_handler_release;
	}
	/*
	 * If we're handling a cos fault, we don't need to the
	 * vma anymore
	 */
	up_read(&curr_mm->mmap_sem);
	mmput(curr_mm);
	cos_meas_event(COS_PG_FAULT);

//	if (get_user_regs_thread(cos_thd_per_core[get_cpuid()].cos_thd) != rs) printk("Nested page fault!\n");
	if (fault_update_mpd_pgtbl(thd, rs, fault_addr)) ret = 0;
	else ret = cos_handle_page_fault(thd, fault_addr, error_code, rs);

	return ret;
linux_handler_release:
	up_read(&curr_mm->mmap_sem);
linux_handler_put:
	mmput(curr_mm);
linux_handler:
	return ret;
}

/*
 * This function will be called upon a hardware div fault.  Return 0
 * if you want the linux fault handler to be run, !0 otherwise.
 */
__attribute__((regparm(3)))
int main_div_fault_interposition(struct pt_regs *rs, unsigned int error_code)
{
	struct thread *t;

	if (cos_thd_per_core[get_cpuid()].cos_thd != current) return 1;

	t = cos_get_curr_thd();
	cos_record_fault_regs(t, error_code, error_code, rs);
	fault_ipc_invoke(t, rs->ip, 0, rs, COS_FLT_DIVZERO);

	return 0;
}

__attribute__((regparm(3))) int
main_reg_save_interposition(struct pt_regs *rs, unsigned int error_code)
{
	struct thread *t;
	struct spd *s;

	if (unlikely(cos_thd_per_core[get_cpuid()].cos_thd != current)) return 1;

	t = cos_get_curr_thd();
	memcpy(&t->fault_regs, rs, sizeof(struct pt_regs));
	/* The spd that was invoked should be the one faulting here
	 * (must get stack) */
	s = thd_curr_spd_noprint();

	return 0;
}

__attribute__((regparm(3))) int
main_fpu_not_available_interposition(struct pt_regs *rs, unsigned int error_code)
{
        if (unlikely(cos_thd_per_core[get_cpuid()].cos_thd != current)) return 1;

        if (syscalls_enabled == 1) return 1;

        return fpu_disabled_exception_handler();
}

void cos_ipi_handling(void);
void cos_cap_ipi_handling(void);

int
cos_ipi_ring_enqueue(u32_t dest, u32_t data);

u64_t sum = 0, ii = 0;
__attribute__((regparm(3))) void
main_ipi_handler(struct pt_regs *rs, unsigned int irq)
{
//	u64_t s,e;

	/* ack the ipi first. */
	ack_APIC_irq();

//	rdtscll(s);
	cos_cap_ipi_handling();
//	rdtscll(e);
	/* if (get_cpuid() == 20) { */
	/* 	ii++; */
	/* 	sum += (e-s); */
	/* 	if (ii % (1024*1024) == 0) { */
	/* 		printk(".......................rcv cost %d\n", sum / (1024*1024)); */
	/* 		sum = 0; */
	/* 	} */
	/* } */

        return;
}

/*
 * Memory semaphore already held.
 */
struct mm_struct* module_page_fault(unsigned long address)
{
	/* In the executive!, the executive's software mappings. */
	if (test_thread_flag(TIF_HIJACK_ENV) && //trusted_mm &&
	    !test_thread_flag(TIF_VIRTUAL_SYSCALL) &&
	    find_vma(trusted_mm, address)) {
		printk("cos: fault in executive: %x\n", (unsigned int)address);
		return trusted_mm;
	}

	return current->mm;
}

/*
 * A pointer to the kernel page table mappings.  This is an entire
 * page table pgd, but only the kernel mappings should be present.
 */
vaddr_t kern_pgtbl_mapping;
struct mm_struct *kern_mm;
int kern_handle;

/*
 * FIXME: error checking
 */
void *chal_alloc_page(void)
{
	void *page = (void*)__get_free_pages(GFP_KERNEL, 0);

	memset(page, 0, PAGE_SIZE);

	return page;
}

void *chal_alloc_kern_mem(int order)
{
	void *page = (void*)__get_free_pages(GFP_KERNEL, order);

	if (!page) return NULL;

	memset(page, 0, PAGE_SIZE * (1<<order));
	cos_kmem_base = cos_kmem = page;

	return page;
}

void chal_free_page(void *page)
{
	free_pages((unsigned long int)page, 0);
}

void chal_free_kern_mem(void *mem, int order)
{
	assert(mem && (mem == cos_kmem_base));
	free_pages((unsigned long int)mem, order);
}

/*
 * FIXME: types for these are messed up.  This is due to difficulty in
 * using them both in the composite world and in the Linux world.  We
 * should just use them in the composite world and be done with it.
 */
paddr_t chal_va2pa(void *va)
{
	return (paddr_t)__pa(va);
}

void *chal_pa2va(paddr_t pa)
{
	if (pa >= COS_MEM_START) return NULL;

	return (void*)__va(pa);
}

/***** begin timer/net handling *****/

/*
 * Our composite emulated timer interrupt executed from a Linux
 * softirq
 */
extern struct thread *ainv_next_thread(struct async_cap *acap, struct thread *preempted, int preempt);

extern void cos_net_deregister(struct cos_net_callbacks *cn_cb);
extern void cos_net_register(struct cos_net_callbacks *cn_cb);
extern int cos_net_try_acap(struct cos_net_acap_info *net_info, void *data, int len);
extern void cos_net_meas_packet(void);
extern int cos_net_notify_drop(struct async_cap *acap);
EXPORT_SYMBOL(cos_net_deregister);
EXPORT_SYMBOL(cos_net_register);
EXPORT_SYMBOL(cos_net_try_acap);
EXPORT_SYMBOL(cos_net_meas_packet);
EXPORT_SYMBOL(cos_net_notify_drop);
extern void cos_net_init(void);
extern void cos_net_finish(void);

extern void cos_trans_reg(const struct cos_trans_fns *fns);
extern void cos_trans_dereg(void);
extern void cos_trans_upcall(void *acap);
EXPORT_SYMBOL(cos_trans_reg);
EXPORT_SYMBOL(cos_trans_dereg);
EXPORT_SYMBOL(cos_trans_upcall);

#define NUM_NET_ACAPS 2 /* keep consistent with inv.c */

CACHE_ALIGNED static int in_syscall[NUM_CPU] = { 0 };

int host_in_syscall(void)
{
	return in_syscall[get_cpuid()];
}

void host_start_syscall(void)
{
	in_syscall[get_cpuid()] = 1;
}
EXPORT_SYMBOL(host_start_syscall);

void host_end_syscall(void)
{
	in_syscall[get_cpuid()] = 0;
}
EXPORT_SYMBOL(host_end_syscall);

/*
 * If we are asleep in idle, or are waking, that is an indicator that
 * the registers aren't arranged in pt_regs formation at the top of
 * the thread's stack.
 */
typedef enum {
	IDLE_AWAKE,	        /* on the Linux scheduler's runqueue */
	IDLE_ASLEEP, 		/* blocked on the hijack_waitq */
	IDLE_WAKING		/* on the Linux runqueue, but haven't
				 * been executed yet (i.e. don't try
				 * to remove us from the waitq
				 * again!) */
} idle_status_t;
static volatile int idle_status = IDLE_AWAKE;

/* is the register state of the thread defined by the idle
 * procedures? I.e. are we either asleep, or waking */
int host_in_idle(void)
{
	return IDLE_AWAKE != idle_status;
}

static inline void sti(void)
{
	__asm__("sti");
}

static inline void cli(void)
{
	__asm__("cli");
}

void
chal_idle(void)
{
	/* set state must be before in_idle=1 to avert race */
	set_current_state(TASK_INTERRUPTIBLE);
	assert(IDLE_AWAKE == idle_status);
	idle_status = IDLE_ASLEEP;
	event_record("going into idle", thd_get_id(cos_get_curr_thd()), 0);
	cos_meas_event(COS_MEAS_IDLE_SLEEP);
	sti();

	/* forfeit execution to Linux */
	schedule();
	cli();
	/* FIXME: If we cntl-c, this will trip...would like to keep
	 * the assert, but clean up signal termination */
	//assert(IDLE_WAKING == idle_status);
	idle_status = IDLE_AWAKE;
	cos_meas_event(COS_MEAS_IDLE_RUN);
	event_record("coming out of idle", thd_get_id(cos_get_curr_thd()), 0);
}

static void
host_idle_wakeup(void)
{
	assert(host_in_idle());
	if (likely(cos_thd_per_core[get_cpuid()].cos_thd)) {
		if (IDLE_ASLEEP == idle_status) {
			cos_meas_event(COS_MEAS_IDLE_LINUX_WAKE);
			event_record("idle wakeup", thd_get_id(cos_get_curr_thd()), 0);
			wake_up_process(cos_thd_per_core[get_cpuid()].cos_thd);
			idle_status = IDLE_WAKING;
		} else {
			cos_meas_event(COS_MEAS_IDLE_RECURSIVE_WAKE);
			event_record("idle wakeup call while waking", thd_get_id(cos_get_curr_thd()), 0);
		}
		assert(IDLE_WAKING == idle_status);
	}
}

int chal_attempt_ainv(struct async_cap *acap)
{
	struct pt_regs *regs = NULL;
	unsigned long flags;

	local_irq_save(flags);
	if (likely(cos_thd_per_core[get_cpuid()].cos_thd)/* == current*/) {
		struct thread *cos_current;

		if (cos_thd_per_core[get_cpuid()].cos_thd == current) {
			cos_meas_event(COS_MEAS_INT_COS_THD);
		} else {
			cos_meas_event(COS_MEAS_INT_OTHER_THD);
		}

		cos_current = cos_get_curr_thd();
		/* See comment in cosnet.c:cosnet_xmit_packet */
		if (host_in_syscall() || host_in_idle()) {
			struct thread *next;

			/*
			 * _FIXME_: Here we are kludging a problem over.
			 * The problem is this:
			 *
			 * First, a thread xmits a packet through
			 * buff_mgmt->cosnet_xmit_packet
			 *
			 * Second, when do_softirq is invoked, it
			 * picks up another pending arrival (somehow)
			 *
			 * Third, this causes the upcall thread to be
			 * executed, and we have the choice to either
			 * mark the current thread as preempted (which
			 * isn't necessarily wise as we haven't stored
			 * e.g. segment registers), or to just save
			 * ecx and edx, and don't mark it as preempted
			 * so that switch_thread will return to it
			 * gracefully.  In either case, the previous
			 * thread is added to the preemption chain of
			 * the upcall thread so that it can be
			 * immediately switched back to.
			 *
			 * The problem occurs when we do use that
			 * preemption chain and in pop(), we attempt
			 * to return to the (assumed preempted)
			 * previous thread in the preemption chain.
			 * Now we are restoring _all_ registers for a
			 * thread where only ecx and edx (and eax)
			 * were saved.  Certainly behavior that will
			 * lead to a wierd fault.
			 *
			 * So the fix is to just not set add the
			 * xmitting thread to the preemption chain.
			 * This will sacrifice performance, which may
			 * be an issue later on.
			 */
			next = ainv_next_thread(acap, cos_current, 0);
			if (next != cos_current) {
				assert(cos_get_curr_thd() == next);
				/* the following call isn't
				 * necessary: if we are in a syscall,
				 * then we can't be in an RAS */
				thd_check_atomic_preempt(cos_current);
			}
			if (host_in_syscall()) {
				cos_meas_event(COS_MEAS_INT_PREEMPT);
				cos_meas_event(COS_MEAS_ACAP_DELAYED_UC);
				event_record("xmit path lead to nested upcalls",
					     thd_get_id(cos_current), thd_get_id(next));
			} else if (host_in_idle()) {
				event_record("upcall causing host idle wakeup",
					     thd_get_id(cos_current), thd_get_id(next));
				host_idle_wakeup();
			}

			goto done;
 		}

		regs = get_user_regs_thread(cos_thd_per_core[get_cpuid()].cos_thd);

		/*
		 * If both esp and xss == 0, then the interrupt
		 * occured between sti; sysexit on the cos ipc/syscall
		 * return path.  If SEGMENT_RPL_MASK is not set to
		 * USER_RPL, then we interrupted kernel-code.  These
		 * are special cases, but we are interested in the
		 * case where we interrupted user-level composite
		 * code.
		 *
		 * I believe it is a BUG if we did NOT interrupt
		 * user-level (keep in mind that we are now looking at
		 * the register set at the top of the stack, not some
		 * interrupt registers or some such.)
		 *
		 * UPDATE: given that we are now accepting network and
		 * timer interrupts, these CAN interrupt each other,
		 * thus we might interrupt kernel-level.  FIXME: make
		 * sure that if we have interrupted kernel-level that
		 * the regs aren't spread across the main thread
		 * stack, and the interrupt's saved registers as well.
		 */
		if (likely(!(regs->sp == 0 && regs->ss == 0))
                    /* && (regs->xcs & SEGMENT_RPL_MASK) == USER_RPL*/) {
			struct thread *next;

			if ((regs->cs & SEGMENT_RPL_MASK) == USER_RPL) {
				cos_meas_event(COS_MEAS_INT_PREEMPT_USER);
			} else {
				cos_meas_event(COS_MEAS_INT_PREEMPT_KERN);
			}

			/* the major work here: */
			next = ainv_next_thread(acap, cos_current, 1);
			if (next != cos_current) {
				thd_save_preempted_state(cos_current, regs);
				if (!(next->flags & THD_STATE_ACTIVE_UPCALL)) {
					printk("cos: upcall thread %d is not set to be an active upcall.\n",
					       thd_get_id(next));
					///*assert*/BUG_ON(!(next->flags & THD_STATE_ACTIVE_UPCALL));
				}
				thd_check_atomic_preempt(cos_current);

				/* Those registers are saved in the
				 * user space. No need to restore
				 * here. */
				/* regs->bx = next->regs.bx; */
				/* regs->di = next->regs.di; */
				/* regs->si = next->regs.si; */
				/* regs->bp = next->regs.bp; */

				regs->cx = next->regs.cx;
				regs->ip = next->regs.ip;
				regs->dx = next->regs.dx;
				regs->ax = next->regs.ax;
				regs->orig_ax = next->regs.ax;
				regs->sp = next->regs.sp;

				//cos_meas_event(COS_MEAS_ACAP_UC);
			}
			cos_meas_event(COS_MEAS_INT_PREEMPT);

			event_record("normal (non-syscall/idle interrupting) upcall processed",
				     thd_get_id(cos_current), thd_get_id(next));
		} else {
			cos_meas_event(COS_MEAS_INT_STI_SYSEXIT);
		}
	}
done:
	local_irq_restore(flags);

	return 0;
}

int chal_attempt_arcv(struct cap_arcv *arcv)
{
	struct pt_regs *regs = NULL;
	unsigned long flags;
	struct thread *thd;
	struct cos_cpu_local_info *cos_info = cos_cpu_local_info();
	struct thread *cos_current;

	local_irq_save(flags);

	if (likely(cos_thd_per_core[get_cpuid()].cos_thd)) {
		thd = arcv->thd;
		if (thd->flags & THD_STATE_ACTIVE_UPCALL) {
			/* handling thread is active. */
			arcv->pending++;
			goto done;
		}

		cos_current = thd_current(cos_info);

		if (cos_thd_per_core[get_cpuid()].cos_thd == current) {
			cos_meas_event(COS_MEAS_INT_COS_THD);
		} else {
			cos_meas_event(COS_MEAS_INT_OTHER_THD);
		}

		regs = get_user_regs_thread(cos_thd_per_core[get_cpuid()].cos_thd);

		if (likely(!(regs->sp == 0 && regs->ss == 0))) {
			/* FIXME: we always switch to upcall currently. */
			struct thread *next = arcv->thd; //FIXME: get the highest prio thread.

			if (next != cos_current) {
				if (likely(chal_pgtbl_can_switch())) {
#ifdef UPDATE_LINUX_MM_STRUCT
					chal_pgtbl_switch((paddr_t)thd_current_pgtbl(next));
#else
					native_write_cr3((unsigned long)thd_current_pgtbl(next));
#endif
				} else {
					/* we are omitting the native_write_cr3 to switch
					 * page tables */
					__chal_pgtbl_switch((paddr_t)thd_current_pgtbl(next));
				}

				thd_preemption_state_update(cos_current, next, regs);

				next->flags |= THD_STATE_ACTIVE_UPCALL;
				next->flags &= ~THD_STATE_READY_UPCALL;

				copy_gp_regs(&next->regs, regs);

				/* and additional regs */
				regs->ip = next->regs.ip;
				regs->sp = next->regs.sp;
				regs->orig_ax = next->regs.ax;

				thd_current_update(next, cos_current, cos_info);
				//cos_meas_event(COS_MEAS_ACAP_UC);
			}

		}

	}
done:
	local_irq_restore(flags);

	return 0;
}

void chal_send_ipi(int cpuid) {
#if defined(CONFIG_X86_LOCAL_APIC)
	/* lowest-level IPI sending. the __default_send function is in
	 * arch/x86/include/asm/ipi.h */

	unsigned int cpu;
	cpu = cpumask_next((unsigned int)-1, cpumask_of(cpuid));

	__default_send_IPI_dest_field(
		apic->cpu_to_logical_apicid(cpu), COS_IPI_VECTOR,
		apic->dest_logical);
#elif defined(CONFIG_BIGSMP)
	/* If BIGSMP is set, use following implementation! above is a
	 * shortcut. */
	/* apic->send_IPI_mask(cpumask_of(cpuid), COS_IPI_VECTOR); */
#endif
}

PERCPU_VAR(cos_timer_acap);

/* hack to detect timer interrupt. */
char timer_detector[PAGE_SIZE] PAGE_ALIGNED;

struct tlb_quiescence tlb_quiescence[NUM_CPU] CACHE_ALIGNED;

__attribute__((regparm(3)))
int main_timer_interposition(struct pt_regs *rs, unsigned int error_code)
{
	struct async_cap *acap = *PERCPU_GET(cos_timer_acap);
	int curr_cpu = get_cpuid();

	u32_t *ticks = (u32_t *)&timer_detector[curr_cpu * CACHE_LINE];

	/* TLB quiescence period. */
	chal_flush_tlb();

	/* Update timestamps for tlb flushes. */
	rdtscll(tlb_quiescence[curr_cpu].last_periodic_flush);
	tlb_quiescence[curr_cpu].last_mandatory_flush = tlb_quiescence[curr_cpu].last_periodic_flush;

	cos_faa(ticks, 1);
	cos_mem_fence();

	if (!(acap && acap->upcall_thd)) goto LINUX_HANDLER;

	/* FIXME: Right now we are jumping back to the Linux timer
	 * handler (which will do the ack()). Linux will freeze if we
	 * don't do this. We should find a way to get rid of this,
	 * possibly by using tickless kernel, which probably could
	 * survive timer hijacking. */

	/* ack_APIC_irq(); */

	chal_attempt_ainv(acap);

	/* return 0; */
LINUX_HANDLER:
	return 1;
}

/***** end timer handling *****/

void thd_publish_data_page(struct thread *thd, vaddr_t page)
{
	unsigned int id = thd_get_id(thd);

	//assert(0 != id && 0 == (page & ~PAGE_MASK));

	//printk("cos: shared_region_pte is %p, page is %x.\n", shared_region_pte, page);
	/* PGTBL_PRESENT is not set */
	((pte_t*)shared_region_page)[id].pte_low = (vaddr_t)chal_va2pa((void*)page) |
		(PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_USER | PGTBL_ACCESSED);

	return;
}

static int open_checks(void)
{
	/*
	 * All of the volatiles are thrown in here because gcc is
	 * getting too get for its own good, and when testing
	 * consistency across different regions in shared memory, we
	 * have to be sure we are in fact accessing the memory (not a
	 * register).
	 */
#define MAGIC_VAL_TEST 0xdeadbeef
	volatile unsigned int *region_ptr;
	paddr_t modval, userval;
	volatile vaddr_t kern_data;

	kern_data = chal_pgtbl_vaddr2kaddr((paddr_t)chal_va2pa(current->mm->pgd), (unsigned long)shared_data_page);
	modval  = (paddr_t)chal_va2pa((void *)kern_data);
	userval = (paddr_t)chal_va2pa((void *)chal_pgtbl_vaddr2kaddr((paddr_t)chal_va2pa(current->mm->pgd),
								     (unsigned long)COS_INFO_REGION_ADDR));
	if (modval != userval) {
		printk("shared data page error: %x != %x\n", (unsigned int)modval, (unsigned int)userval);
		return -EFAULT;
	}
	region_ptr  = (unsigned int *)COS_INFO_REGION_ADDR;
	*((volatile unsigned int*)shared_data_page) = MAGIC_VAL_TEST;
	*((volatile unsigned int*)region_ptr) = MAGIC_VAL_TEST;
	if (*region_ptr != *shared_data_page || *region_ptr != MAGIC_VAL_TEST) {
		printk("cos: Mapping of the cos shared region didn't work (%x != %x !=(kern page) %x).\n",
		       (unsigned int)*region_ptr, (unsigned int)*shared_data_page, *(unsigned int*)kern_data);
		return -EFAULT;
	} else {
		printk("cos: Mapping of shared region worked: %x.\n", (unsigned int)*region_ptr);
	}

	*region_ptr = 0;
	return 0;
}

static
void init_globals(void)
{
	int cpuid;

	shared_region_pte  = NULL;
	union_pgd          = NULL;
	for (cpuid = 0; cpuid < NUM_CPU; cpuid++) {
		cos_thd_per_core[cpuid].cos_thd = NULL;
	}
	composite_union_mm = NULL;
	kern_mm            = NULL;
	kern_pgtbl_mapping = 0;
	kern_handle        = 0;
	idle_status        = IDLE_AWAKE;
}

/*
 * Opening the aed device signals the intended use of the Composite
 * operating system along side the currently executing Linux.  Thus,
 * when the fd is open, we must prepare the virtual address space for
 * COS use.
 */
static void init_guest_mm_vect(void);
static int aed_open(struct inode *inode, struct file *file)
{
	pte_t *pte = lookup_address_mm(current->mm, COS_INFO_REGION_ADDR);
	pgd_t *pgd;
	void* data_page;

	init_globals();

	cos_kern_stk_init();

	if (cos_thd_per_core[get_cpuid()].cos_thd != NULL || composite_union_mm != NULL) {
		printk("cos (CPU %d): Composite subsystem already used by %d (%p).\n", get_cpuid(), cos_thd_per_core[get_cpuid()].cos_thd->pid, cos_thd_per_core[get_cpuid()].cos_thd);
		return -EBUSY;
	}

	/* We assume this in one page. */
	assert(sizeof(struct cos_component_information) <= PAGE_SIZE);

	/* Sanity check. These defines should match info from Linux. */
	if ((THREAD_SIZE != THREAD_SIZE_LINUX) ||
	    (CPUID_OFFSET_IN_THREAD_INFO != offsetof(struct thread_info, cpu)) ||
	    (LINUX_THREAD_INFO_RESERVE < sizeof(struct thread_info))) {
		printk("cos: Please check THREAD_SIZE in Linux or thread_info struct.\n");
		return -EFAULT;
	}

	save_per_core_cos_thd();

	syscalls_enabled = 1;
	composite_union_mm = get_task_mm(current);
	union_pgd = composite_union_mm->pgd;

	if (pte != NULL) {
		printk("cos: address range for info region @ %x already used.\n",
 		       (unsigned int)COS_INFO_REGION_ADDR);
		return -ENOMEM;
	}

	kern_handle = aed_allocate_mm();
	kern_mm = aed_get_mm(kern_handle);
	kern_pgtbl_mapping = (vaddr_t)kern_mm->pgd;
	/*
	 * This is really and truly crap, because of Linux.  Linux has
	 * 4 address namespaces, it seems and I was only aware of 3.
	 * 1) physical, 2) kernel virtual, 3) user-level virtual, and
	 * 4) module code and data virtual (roundabout 0xf88...).  If
	 * we want to use a page size region in a module's dataspace
	 * (so that we can access the memory directly without
	 * indirection through a variable), we need to convert using
	 * the page tables, the module virtual address to a physical
	 * address, then to a kernel virtual, so that we can
	 * manipulate it and use __pa on it (without __pa throwing up)
	 *
	 * I'm sure Linux guys (to over-generalize) will scream that
	 * using regions of static memory in my module for page-tables
	 * is horrible, but they would also probably agree that they
	 * aren't familar with the types of optimizations you need to
	 * go through to make a microkernel fast: They'd probably
	 * spend most of their time complaining about microkernels as
	 * being horrible instead.
	 */

	shared_region_pte = (pte_t *)chal_pgtbl_vaddr2kaddr((paddr_t)chal_va2pa(current->mm->pgd),
							  (unsigned long)shared_region_page);
	if (((unsigned long)shared_region_pte & ~PAGE_MASK) != 0) {
		printk("Allocated page for shared region not page aligned.\n");
		return -EFAULT;
	}
	memset(shared_region_pte, 0, PAGE_SIZE);

	/* hook in the data page */
	data_page = (void *)chal_va2pa((void *)chal_pgtbl_vaddr2kaddr((paddr_t)chal_va2pa(current->mm->pgd),
							   (unsigned long)shared_data_page));
	shared_region_pte[0].pte_low = (unsigned long)(data_page) |
		(PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_USER | PGTBL_ACCESSED);
	/* hook up the actual virtual memory pages to the pte
	 * protection mapping equivalent to PAGE_SHARED */
/*	for (i = 0 ; i < MAX_NUM_THREADS+1 ; i++) {
		shared_region_pte[i].pte_low = (unsigned long)(__pa(pages_ptr+(PAGE_SIZE*i))) |
			(PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_USER | PGTBL_ACCESSED);
	}
*/

	/* Where in the page directory should the pte go? */
	pgd = pgd_offset(current->mm, COS_INFO_REGION_ADDR);
	if (pgd_none(*pgd)) {
		printk("Could not get pgd_offset.\n");
		return -EFAULT;
	}
	/* hook up the pte to the pgd */
	pgd->pgd = (unsigned long)(__pa(shared_region_pte)) | _PAGE_TABLE;

	/*
	 * This is used to copy valid (linux) kernel mappings into a
	 * new mpd.  Copy shared region too.
	 */
	pgd = pgd_offset(kern_mm, COS_INFO_REGION_ADDR);
	if (pgd_none(*pgd)) {
		printk("Could not get pgd_offset in the kernel map.\n");
		return -EFAULT;
	}
	pgd->pgd = (unsigned long)(__pa(shared_region_pte)) | _PAGE_TABLE;

	printk("cos: info region @ %d(%x)\n",
	       COS_INFO_REGION_ADDR, COS_INFO_REGION_ADDR);

	if (open_checks()) return -EFAULT;

	thd_init();
	spd_init();
	ipc_init();
	if (cos_init_memory()) return -EFAULT;

	cos_meas_init();
	cos_net_init();

	return 0;
}

//extern void event_print(void);
static int aed_release(struct inode *inode, struct file *file)
{
	pgd_t *pgd;
	int i;
#ifdef FAULT_DEBUG
	int j, k;
#endif

	/*
	 * when the aed control file descriptor is closed, lets get
	 * rid of all resources the aed environment was using, but
	 * only if something was promoted in the first place.
	 *
	 * This is our method of cleaning up the garbage.
	 *
	 * FIXME: this should all be synchronized around with the mm
	 * semaphore.
	 */
	if (test_thread_flag(TIF_HIJACK_ENV)) {
		clear_ti_thread_flag(current_thread_info(), TIF_HIJACK_ENV);

		current->mm = trusted_mm;
		current->active_mm = trusted_mm;

		/* Let another process create a asym environment */
		trusted_mm = NULL;

		remove_all_guest_mms();
		flush_all(current->mm->pgd);
		BUG();
	}

	trusted_mm = NULL;
	remove_all_guest_mms();

	cos_net_finish();

#ifdef KMEM_HACK
	for (i = 0; i < bootkmem_used; i++) {
		if (bootkmem[i]) {
			chal_free_page(bootkmem[i]);
			bootkmem[i] = 0;
		} else {
			break;
		}
	}
#endif
	/* our garbage collection mechanism: all at once when the cos
	 * system control fd is closed */
//	thd_free(cos_get_curr_thd());
	thd_free_all();
// 	thd_init();
	spd_free_all();
	ipc_init();
	cos_shutdown_memory();
	cos_meas_report();

	/* reset the address space to the original process */
	composite_union_mm->pgd = union_pgd;

	/*
	 * free the shared region...
	 * FIXME: should also kill the actual pages of shared memory
	 */
	pgd = pgd_offset(composite_union_mm, COS_INFO_REGION_ADDR);
	if (pgd) memset(pgd, 0, sizeof(int));

	syscalls_enabled = 1;
	for (i = 0 ; i < NUM_CPU ; i++) {
		in_syscall[i] = 0;
	}

	/*
	 * Keep the mm_struct around till we have gotten rid of our
	 * cos-specific mappings.  This is required as in do_exit, mm
	 * is dropped before files and fs (thus current->mm should not
	 * be accessed from fd release procedures.)
	 */
	mmput(composite_union_mm);
	init_globals();

#ifdef FAULT_DEBUG
	printk("cos: Page fault information:\n");
	printk("cos: Number of buckets %d, bucket mask %x.\n",
	       (int)NUM_BUCKETS, (unsigned int)BUCKET_MASK);
	#define PER_ROW 8
	for (i = 0, k = 0 ; i < NUM_BUCKETS/PER_ROW ; i++) {
		printk("cos: %d - ", i);
		for (j = 0 ; j < PER_ROW ; j++, k++) {
			printk("%12u", (unsigned int)fault_addrs[k]);
		}
		printk("\n");
	}
#endif
	{
		int i;
		printk("\ncos: Faults:\n");
		for (i = (fault_ptr+1)%NFAULTS ; i != fault_ptr ; i = (i + 1) % NFAULTS) {
			struct fault_info *fi = &faults[i];

			if (fi->thdid != 0) {
				printk("cos: spd %d, thd %d @ addr %x w/ flags %x @ time %lld, mpd flags %x (master %x) and w/ regs: \ncos:\t\t"
				       "eip %10x, esp %10x, eax %10x, ebx %10x, ecx %10x,\ncos:\t\t"
				       "edx %10x, edi %10x, esi %10x, ebp %10x,\n"
				       "cos:\t\tcs  %10x, ss  %10x, flags %10x\n",
				       fi->spdid, fi->thdid,
				       (unsigned int)fi->addr,
				       fi->err_code,
				       fi->timestamp,
				       fi->cspd_flags,
				       fi->cspd_master_flags,
				       (unsigned int)fi->regs.ip,
				       (unsigned int)fi->regs.sp,
				       (unsigned int)fi->regs.ax,
				       (unsigned int)fi->regs.bx,
				       (unsigned int)fi->regs.cx,
				       (unsigned int)fi->regs.dx,
				       (unsigned int)fi->regs.di,
				       (unsigned int)fi->regs.si,
				       (unsigned int)fi->regs.bp,
				       (unsigned int)fi->regs.cs,
				       (unsigned int)fi->regs.ss,
				       (unsigned int)fi->regs.flags);
				fi->thdid = 0; /* reset */
			}
		}
		event_print();
	}

	/* Redundant, but needed when exit with Ctrl-C */
	hw_reset(NULL);
	smp_call_function(hw_reset, NULL, 1);

	return 0;
}

static struct file_operations proc_aed_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = aed_ioctl,
	.open           = aed_open,
	.release        = aed_release,
};

/*Macro-ify it to avoid the duplication.*/
static int make_proc_aed(void)
{
	struct proc_dir_entry *ent;

	ent = create_proc_entry("aed", 0222, NULL);
	if(ent == NULL){
		printk("cos: make_proc_aed : Failed to register /proc/aed\n");
		return -1;
	}
	ent->proc_fops = &proc_aed_fops;

	return 0;
}

static void hw_init_CPU(void)
{
	//update_vmalloc_regions();
	hw_int_init();

	return;
}

static int asym_exec_dom_init(void)
{
	printk("cos (core %d): Installing the hijack module.\n", get_cpuid());
	/* pt_regs in this linux version has changed... */
	BUG_ON(sizeof(struct pt_regs) != (17*sizeof(long)));

	if (make_proc_aed())
		return -1;

	hw_init_CPU();

	/* Consistency check. We define the THD_REGS = 8 in ipc.S. */
	BUG_ON(offsetof(struct thread, regs) != 8);

	init_guest_mm_vect();
	trusted_mm = NULL;

	return 0;
}

static void asym_exec_dom_exit(void)
{
	remove_proc_entry("aed", NULL);

	return;
}

module_init(asym_exec_dom_init);
module_exit(asym_exec_dom_exit);

MODULE_AUTHOR("Gabriel Parmer");
MODULE_DESCRIPTION("Composite Operating System support module for coexistence with Linux");
MODULE_LICENSE("GPL");
