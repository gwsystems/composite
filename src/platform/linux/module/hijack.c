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
/* fget */
#include <linux/file.h>
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

#include "./hw_ints.h"

MODULE_LICENSE("GPL");
#define MODULE_NAME "asymmetric_execution_domain_support"

extern void sysenter_interposition_entry(void);
extern void page_fault_interposition(void);
extern void div_fault_interposition(void);
extern void state_inv_interposition(void);

/* extern unsigned long cos_default_page_fault_handler; */
/* extern unsigned long cos_default_div_fault_handler; */
/* extern unsigned long cos_default_state_inv_handler; */
//extern void *cos_sysenter_addr;

/* 
 * This variable exists for the assembly code for temporary
 * storage...want it close to sysenter_addr for cache locality
 * purposes.
 */
extern unsigned long temp_esp_storage; 

/* 
 * This variable is the page table entry that links to all of the
 * shared region data including the read-only information page and the
 * data region for passing arguments and holding persistent data.
 */
pte_t *shared_region_pte;
pgd_t *union_pgd;

struct task_struct *composite_thread = NULL;
struct mm_struct *composite_union_mm = NULL;

/* composite: should be in separate module */
unsigned long jmp_addr __attribute__((aligned(32))) = 0 ;
unsigned long stub_addr = 0;
unsigned long saved_ip, saved_ret_ip;
unsigned long saved_sp;
unsigned long return_ip;
unsigned long saved_cr3;

/* 
 * These are really a per-thread resource (per CPU if we assume hijack
 * only runs one thread per CPU).  These data structures have been
 * taken out of the task struct to improve locality, and so that we
 * don't have to modify the linux kernel (one cannot patch
 * data-structures in some magic way in an already compiled kernel to
 * include extra fields).
 */
struct mm_struct *trusted_mm = NULL;
/* 
 * If we are in a guest's address space, currently executing in the
 * executive, we have a page fault within the executive range, and we
 * switch to the trusted mm, we need record which guest address space
 * we should be executing in with the following variable. 
 */
struct mm_struct *current_active_guest = NULL;
struct pt_regs *user_level_regs;
struct pt_regs trusted_regs;
unsigned long trusted_mem_limit;
unsigned long trusted_mem_size;


#define MAX_ALLOC_MM 64
struct mm_struct *guest_mms[MAX_ALLOC_MM]; 

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
	if (spd->location.lowest_addr == 0) return 0;

	mm = guest_mms[spd->local_mmaps];
	pgd = pgd_offset(mm, spd->location.lowest_addr);
	span = spd->location.size>>HPAGE_SHIFT;

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
	unsigned int span = size>>HPAGE_SHIFT;

#ifdef NIL
	if (!(pgd_val(*fpgd) & _PAGE_PRESENT)) {
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

static int syscalls_enabled = 1;

extern int virtual_namespace_alloc(struct spd *spd, unsigned long addr, unsigned int size);
void zero_pgtbl_range(paddr_t pt, unsigned long lower_addr, unsigned long size);
void copy_pgtbl_range(paddr_t pt_to, paddr_t pt_from, 
		      unsigned long lower_addr, unsigned long size);
void copy_pgtbl(paddr_t pt_to, paddr_t pt_from);
//extern int copy_mm(unsigned long clone_flags, struct task_struct * tsk);
void print_valid_pgtbl_entries(paddr_t pt);
extern struct thread *ready_boot_thread(struct spd *init);
vaddr_t pgtbl_vaddr_to_kaddr(paddr_t pgtbl, unsigned long addr);

static int aed_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch(cmd) {
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
			spd->spd_info.pg_tbl = (paddr_t)(__pa(current->mm->pgd));
			spd->location.lowest_addr = 0;
			spd->composite_spd = &spd->spd_info;
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
			assert(spd == virtual_namespace_query(spd_info.lowest_addr+PAGE_SIZE));

			copy_pgd_range(mm, current->mm, spd_info.lowest_addr, spd_info.size);
			copy_pgd_range(mm, current->mm, COS_INFO_REGION_ADDR, PGD_RANGE);

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

/*
			copy_pgtbl(cspd->spd_info.pg_tbl, __pa(mm->pgd));
			cspd->spd_info.pg_tbl = __pa(mm->pgd);
////			spd->composite_spd = &spd->spd_info;//&cspd->spd_info;
			spd->composite_spd = &cspd->spd_info;
*/
#ifdef NIL
			/* To check the integrity of the created page table: */
			void print_valid_pgtbl_entries(paddr_t pt);
			print_valid_pgtbl_entries(spd->composite_spd->pg_tbl);
#endif
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
		return cap_no;

	}
	case AED_CREATE_THD:
	{
		struct cos_thread_info thread_info;
		struct thd_sched_info *tsi;
		struct thread *thd;
		struct spd *spd;

		if (copy_from_user(&thread_info, (void*)arg, 
				   sizeof(struct cos_thread_info))) {
			//printk("cos: Error copying thread_info from user.\n");
			return -EFAULT;
		}

		spd = spd_get_by_index(thread_info.spd_handle);
		if (!spd) {
			printk("cos: Spd %d invalid for thread creation.\n", 
			       thread_info.spd_handle);
			return -EINVAL;
		}

		thd = ready_boot_thread(spd);

		spd = spd_get_by_index(thread_info.sched_handle);
		if (!spd) {
			printk("cos: scheduling spd %d not permitted to create thread.\n", 
			       thread_info.sched_handle);
			thd_free(thd);
			return -EINVAL;
		}
		
		tsi = thd_get_sched_info(thd, 0);
		tsi->scheduler = spd;

		/* FIXME: need to return opaque handle, rather than
		 * just set the current thread to be the new one. */

		return 0;
	}
	case AED_CAP_CHANGE_ISOLATION:
	{
		isolation_level_t prev_lvl;
		struct cap_info cap_info;

		if (copy_from_user(&cap_info, (void*)arg, 
				   sizeof(struct cap_info))) {
			//printk("cos: Error copying cap_info from user.\n");
			return -EFAULT;
		}
		
		if (cap_is_free(cap_info.cap_handle)) {
			return -EINVAL;
		}

		if (cap_info.il > MAX_ISOLATION_LVL_VAL) {
			return -EINVAL;
		}
		
		prev_lvl = cap_change_isolation(cap_info.cap_handle, cap_info.il, cap_info.flags);

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

//		printk("cos: promoting component %d to scheduler at depth %d, and parent %d\n",
//		       sched_info.spd_sched_handle, sched->sched_depth, sched_info.spd_parent_handle);

		if (sched_info.sched_shared_page < sched->location.lowest_addr ||
		    sched_info.sched_shared_page + PAGE_SIZE >= 
		    sched->location.lowest_addr + sched->location.size) {
			/* undo changes made so far */
			sched->sched_depth = -1;
			sched->parent_sched = NULL;

			printk("cos: could not promote spd %d to scheduler - invalid pinned page @ %x.\n",
			       spd_get_index(sched), (unsigned int)sched_info.sched_shared_page);
			return -EINVAL;
		} 

		sched->sched_shared_page = (struct cos_sched_data_area *)sched_info.sched_shared_page;
		/* We will need to access the shared_page for thread
		 * events when the pagetable for this spd is not
		 * mapped in.  */
		sched->kern_sched_shared_page = (struct cos_sched_data_area *)
			pgtbl_vaddr_to_kaddr(sched->spd_info.pg_tbl, (unsigned long)sched->sched_shared_page);
		sched->prev_notification = 0;
			
		return 0;
	}
	case AED_EMULATE_PREEMPT:
	{
		struct pt_regs *regs = get_user_regs_thread(composite_thread);
		struct thread *cos_thd = thd_get_current();
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
	default: 
		ret = -EINVAL;
	}

	return ret;
}

#define NFAULTS 200
int fault_ptr = 0;
struct fault_info {
	vaddr_t addr;
	struct pt_regs regs;
	unsigned short int spdid, thdid;
	int cspd_flags, cspd_master_flags;
	unsigned long long timestamp;
} faults[NFAULTS];

static void cos_report_fault(struct thread *t, vaddr_t fault_addr, struct pt_regs *regs)
{
	struct fault_info *fi;
	unsigned long long ts;
	struct spd_poly *spd_poly;

	rdtscll(ts);

	fi = &faults[fault_ptr];
	fi->addr = fault_addr;
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
static int cos_prelinux_handle_page_fault(struct thread *thd, struct pt_regs *regs, 
					  vaddr_t fault_addr)
{
	struct spd_poly *active = thd_get_thd_spdpoly(thd), *curr;
	struct composite_spd *cspd;
	vaddr_t ucap_addr = regs->ax;
	struct spd *origin;
	struct pt_regs *regs_save;
	
	/* 
	 * If we are in the most up-to-date version of the
	 * page-tables, then there is no fixing up to do, and we
	 * should just return.  Check for this case.
	 */
	assert(active);
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
	origin = virtual_namespace_query(ucap_addr);
	if (unlikely(NULL == origin)) return 0;
	/* up-to-date pd */
	curr = origin->composite_spd;

	/* 2 */
	if (unlikely(pgtbl_entry_absent(active->pg_tbl, ucap_addr))) return 0;

	/* 3: really don't know what could cause this */
	if (unlikely(!pgtbl_entry_absent(active->pg_tbl, fault_addr))) return 0;

	/* 4 */
	if (unlikely(pgtbl_entry_absent(curr->pg_tbl, fault_addr))) return 0;
	
	/* 5
	 *
	 * Extend the current protection domain to include mappings of
	 * a more up-to-date pd if this one is subordinate and not
	 * consistent.
	 */
	copy_pgtbl_range(active->pg_tbl, curr->pg_tbl, fault_addr, HPAGE_SIZE);
	
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
cos_record_fault_regs(struct thread *t, vaddr_t fault_addr, struct pt_regs *rs)
{	
	memcpy(&t->regs, rs, sizeof(struct pt_regs));
	cos_report_fault(t, fault_addr, rs);
}

/* the composite specific page fault handler */
static int 
cos_handle_page_fault(struct thread *thd, vaddr_t fault_addr, struct pt_regs *regs)
{
	cos_record_fault_regs(thd, fault_addr, regs);
	return 1;
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

void hijack_syscall_monitor(void)
{
	if (unlikely(!syscalls_enabled && composite_thread == current)) {
		printk("FAILURE: making a Linux system call in Composite.\n");
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

	/* Composite doesn't know how to handle kernel faults */
	if (PF_KERN(error_code)) goto linux_handler;

	/*
	 * We want to allow composite to handle the fault if we are in
	 * the composite thread and either the fault was outside the
	 * spd's boundaries or there is not a linux mapping for the
	 * address.
	 */
	if (composite_thread != current) goto linux_handler;

	fault_addr = read_cr2();

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
	thd = thd_get_current();
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
	
	if (get_user_regs_thread(composite_thread) != rs) printk("Nested page fault!\n");
	ret = cos_handle_page_fault(thd, fault_addr, rs);

	return ret;
linux_handler_release:
	up_read(&curr_mm->mmap_sem);
linux_handler_put:
	mmput(curr_mm);
linux_handler:
	return ret; 
}

/*
 * This function will be called upon a hardware page fault.  Return 0
 * if you want the linux page fault to be run, !0 otherwise.
 */
__attribute__((regparm(3))) 
int main_div_fault_interposition(struct pt_regs *rs, unsigned int error_code)
{
	struct thread *t;

	if (composite_thread != current) return 1;

	printk("<<< finally >>>\n");

	t = thd_get_current();
	cos_record_fault_regs(t, error_code, rs);

	return 1;
}

__attribute__((regparm(3))) 
int main_state_inv_interposition(struct pt_regs *rs, unsigned int error_code)
{
	struct thread *t;
	struct spd *s;

	if (unlikely(composite_thread != current)) return 1;

	t = thd_get_current();
	memcpy(&t->fault_regs, rs, sizeof(struct pt_regs));
	/* The spd that was invoked should be the one faulting here
	 * (must get stack) */
	s = thd_curr_spd_noprint();
	

	return 0;
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
void *cos_alloc_page(void)
{
	void *page = (void*)__get_free_pages(GFP_KERNEL, 0);
	
	memset(page, 0, PAGE_SIZE);
	
	return page;
}

void cos_free_page(void *page)
{
	__free_pages(page, 0);
}

/*
 * FIXME: types for these are messed up.  This is due to difficulty in
 * using them both in the composite world and in the Linux world.  We
 * should just use them in the composite world and be done with it.
 */
void *va_to_pa(void *va) 
{
	return (void*)__pa(va);
}

void *pa_to_va(void *pa) 
{
	return (void*)__va(pa);
}

static inline pte_t *pgtbl_lookup_address(paddr_t pgtbl, unsigned long addr)
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

#ifdef NIL
void pgtbl_print_tree(paddr_t pgtbl, unsigned long addr)
{
	pgd_t *pt = ((pgd_t *)pa_to_va((void*)pgtbl)) + pgd_index(addr);
	pte_t *pe = pgtbl_lookup_address(pgtbl, addr);
	
	if (!pt || !pe) {
		printk("cos: printing page table error, NULL found\n");
	} else {
		printk("cos: pgd entry -- %x, pte entry -- %x\n", 
		       pgd_val(*pt), pte_val(*pe));
	}

	return;
}
#endif

int pgtbl_add_entry(paddr_t pgtbl, unsigned long vaddr, unsigned long paddr)
{
	pte_t *pte = pgtbl_lookup_address(pgtbl, vaddr);

	if (!pte || pte_val(*pte) & _PAGE_PRESENT) {
		return -1;
	}
	/*pte_val(*pte)*/pte->pte_low = paddr | (_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED);

	return 0;
}

/* allocate and link in a page middle directory */
int pgtbl_add_middledir(paddr_t pt, unsigned long vaddr)
{
	pgd_t *pgd = ((pgd_t *)pa_to_va((void*)pt)) + pgd_index(vaddr);
	unsigned long *page;

	page = cos_alloc_page(); /* zeroed */
	if (!page) return -1;

	pgd->pgd = (unsigned long)va_to_pa(page) | _PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED;
	return 0;
}

/*
 * Remove a given virtual mapping from a page table.  Return 0 if
 * there is no present mapping, and the physical address mapped if
 * there is an existant mapping.
 */
paddr_t pgtbl_rem_ret(paddr_t pgtbl, vaddr_t va)
{
	pte_t *pte = pgtbl_lookup_address(pgtbl, va);
	paddr_t val;

	if (!pte || !(pte_val(*pte) & _PAGE_PRESENT)) {
		return 0;
	}
	val = (paddr_t)(pte_val(*pte) & PTE_MASK);
	pte->pte_low = 0;

	return val;
}

/* 
 * This won't work to find the translation for the argument region as
 * __va doesn't work on module-mapped memory. 
 */
vaddr_t pgtbl_vaddr_to_kaddr(paddr_t pgtbl, unsigned long addr)
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

	kaddr = (unsigned long)__va(pte_val(*pte) & PTE_MASK) + (~PAGE_MASK & addr);

	return (vaddr_t)kaddr;
}

unsigned int *pgtbl_module_to_vaddr(unsigned long addr)
{
	return (unsigned int *)pgtbl_vaddr_to_kaddr((paddr_t)va_to_pa(current->mm->pgd), addr);
}

/*
 * Verify that the given address in the page table is present.  Return
 * 0 if present, 1 if not.  *This will check the pgd, not for the pte.*
 */
int pgtbl_entry_absent(paddr_t pt, unsigned long addr)
{
	pgd_t *pgd = ((pgd_t *)pa_to_va((void*)pt)) + pgd_index(addr);

	return !((pgd_val(*pgd)) & _PAGE_PRESENT);
}

/* Find the nth valid pgd entry */
unsigned long get_valid_pgtbl_entry(paddr_t pt, int n)
{
	int i;

	for (i = 1 ; i < PTRS_PER_PGD ; i++) {
		if (!pgtbl_entry_absent(pt, i*PGDIR_SIZE)) {
			n--;
			if (n == 0) {
				return i*PGDIR_SIZE;
			}
		}
	}
	return 0;
}

void print_valid_pgtbl_entries(paddr_t pt) 
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

void zero_pgtbl_range(paddr_t pt, unsigned long lower_addr, unsigned long size)
{
	pgd_t *pgd = ((pgd_t *)pa_to_va((void*)pt)) + pgd_index(lower_addr);
	unsigned int span = size>>HPAGE_SHIFT;

	if (!(pgd_val(*pgd)) & _PAGE_PRESENT) {
		printk("cos: BUG: nothing to copy from pgd @ %x.\n", 
		       (unsigned int)lower_addr);
	}

	/* sizeof(pgd entry) is intended */
	memset(pgd, 0, span*sizeof(pgd_t));
}

void copy_pgtbl_range(paddr_t pt_to, paddr_t pt_from, 
		      unsigned long lower_addr, unsigned long size)
{
	pgd_t *tpgd = ((pgd_t *)pa_to_va((void*)pt_to)) + pgd_index(lower_addr);
	pgd_t *fpgd = ((pgd_t *)pa_to_va((void*)pt_from)) + pgd_index(lower_addr);
	unsigned int span = size>>HPAGE_SHIFT;

	if (!(pgd_val(*fpgd)) & _PAGE_PRESENT) {
		printk("cos: BUG: nothing to copy from pgd @ %x.\n", 
		       (unsigned int)lower_addr);
	}

	/* sizeof(pgd entry) is intended */
	memcpy(tpgd, fpgd, span*sizeof(pgd_t));
}

void copy_pgtbl_range_nocheck(paddr_t pt_to, paddr_t pt_from, 
			      unsigned long lower_addr, unsigned long size)
{
	pgd_t *tpgd = ((pgd_t *)pa_to_va((void*)pt_to)) + pgd_index(lower_addr);
	pgd_t *fpgd = ((pgd_t *)pa_to_va((void*)pt_from)) + pgd_index(lower_addr);
	unsigned int span = size>>HPAGE_SHIFT;

	/* sizeof(pgd entry) is intended */
	memcpy(tpgd, fpgd, span*sizeof(pgd_t));
}

/* Copy pages non-empty in from, and empty in to */
void copy_pgtbl_range_nonzero(paddr_t pt_to, paddr_t pt_from, 
			      unsigned long lower_addr, unsigned long size)
{
	pgd_t *tpgd = ((pgd_t *)pa_to_va((void*)pt_to)) + pgd_index(lower_addr);
	pgd_t *fpgd = ((pgd_t *)pa_to_va((void*)pt_from)) + pgd_index(lower_addr);
	unsigned int span = size>>HPAGE_SHIFT;
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

void copy_pgtbl(paddr_t pt_to, paddr_t pt_from)
{
	copy_pgtbl_range_nocheck(pt_to, pt_from, 0, 0xFFFFFFFF);
}

/*
 * If for some reason Linux preempts the composite thread, then when
 * it starts it back up again, it needs to know what page tables to
 * use.  Thus update the current mm_struct.
 */
void switch_host_pg_tbls(paddr_t pt)
{
	struct mm_struct *mm;

	BUG_ON(!composite_thread);
	/* 
	 * We aren't doing reference counting here on the mm (via
	 * get_task_mm) because we know that this mm will survive
	 * until the module is unloaded (i.e. it is refcnted at a
	 * granularity of the creation of the composite file
	 * descriptor open/close.)
	 */
	mm = composite_thread->mm;
	mm->pgd = (pgd_t *)pa_to_va((void*)pt);

	return;
}

/***** begin timer/net handling *****/

/* 
 * Our composite emulated timer interrupt executed from a Linux
 * softirq
 */
static struct timer_list timer;

extern struct thread *brand_next_thread(struct thread *brand, struct thread *preempted, int preempt);

extern void cos_net_deregister(struct cos_net_callbacks *cn_cb);
extern void cos_net_register(struct cos_net_callbacks *cn_cb);
extern int cos_net_try_brand(struct thread *t, void *data, int len);
extern void cos_net_prebrand(void);
extern int cos_net_notify_drop(struct thread *brand);
EXPORT_SYMBOL(cos_net_deregister);
EXPORT_SYMBOL(cos_net_register);
EXPORT_SYMBOL(cos_net_try_brand);
EXPORT_SYMBOL(cos_net_prebrand);
EXPORT_SYMBOL(cos_net_notify_drop);
extern void cos_net_init(void);
extern void cos_net_finish(void);

extern struct thread *cos_timer_brand_thd;
#define NUM_NET_BRANDS 2 /* keep consistent with inv.c */
extern int active_net_brands;
extern struct cos_brand_info cos_net_brand[NUM_NET_BRANDS];
extern struct cos_net_callbacks *cos_net_fns;

/* FIXME: per cpu */
static int in_syscall = 0;

int host_in_syscall(void) 
{
	return in_syscall;
}

void host_start_syscall(void)
{
	in_syscall = 1;
}
EXPORT_SYMBOL(host_start_syscall);

void host_end_syscall(void)
{
	in_syscall = 0;
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

void host_idle(void)
{
	/* set state must be before in_idle=1 to avert race */
	set_current_state(TASK_INTERRUPTIBLE);
	assert(IDLE_AWAKE == idle_status);
	idle_status = IDLE_ASLEEP;
	event_record("going into idle", thd_get_id(thd_get_current()), 0);
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
	event_record("coming out of idle", thd_get_id(thd_get_current()), 0);
}

static void host_idle_wakeup(void)
{
	assert(host_in_idle());
	if (likely(composite_thread)) {
		if (IDLE_ASLEEP == idle_status) {
			cos_meas_event(COS_MEAS_IDLE_LINUX_WAKE);
			event_record("idle wakeup", thd_get_id(thd_get_current()), 0);
			wake_up_process(composite_thread);
			idle_status = IDLE_WAKING;
		} else {
			cos_meas_event(COS_MEAS_IDLE_RECURSIVE_WAKE);
			event_record("idle wakeup call while waking", thd_get_id(thd_get_current()), 0);
		}
		assert(IDLE_WAKING == idle_status);
	}
}

int host_can_switch_pgtbls(void) { return current == composite_thread; }

int host_attempt_brand(struct thread *brand)
{
	struct pt_regs *regs = NULL;
	unsigned long flags;

	local_irq_save(flags);
	if (likely(composite_thread)/* == current*/) {
		struct thread *cos_current;

		if (composite_thread == current) {
			cos_meas_event(COS_MEAS_INT_COS_THD);
		} else {
			cos_meas_event(COS_MEAS_INT_OTHER_THD);
		}

		cos_current = thd_get_current();
		/* See comment in cosnet.c:cosnet_xmit_packet */
		if (host_in_syscall() || host_in_idle()) {
			struct thread *next;

			//next = brand_next_thread(brand, cos_current, 2);
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
			next = brand_next_thread(brand, cos_current, 0);
			if (next != cos_current) {
				assert(thd_get_current() == next);
				/* the following call isn't
				 * necessary: if we are in a syscall,
				 * then we can't be in an RAS */
				thd_check_atomic_preempt(cos_current);
			}
			if (host_in_syscall()) {
				cos_meas_event(COS_MEAS_INT_PREEMPT);
				cos_meas_event(COS_MEAS_BRAND_DELAYED_UC);
				event_record("xmit path lead to nested upcalls", 
					     thd_get_id(cos_current), thd_get_id(next));
			} else if (host_in_idle()) {
				event_record("upcall causing host idle wakeup", 
					     thd_get_id(cos_current), thd_get_id(next));
				host_idle_wakeup();
			}

			goto done;
 		} //else if (thd_get_id(brand->upcall_threads) == 13) printk(">r\n");

		regs = get_user_regs_thread(composite_thread);

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
			next = brand_next_thread(brand, cos_current, 1);
			if (next != cos_current) {
				thd_save_preempted_state(cos_current, regs);
				if (!(next->flags & THD_STATE_ACTIVE_UPCALL)) {
					printk("cos: upcall thread %d is not set to be an active upcall.\n",
					       thd_get_id(next));
					///*assert*/BUG_ON(!(next->flags & THD_STATE_ACTIVE_UPCALL));
				}
				thd_check_atomic_preempt(cos_current);
				regs->bx = next->regs.bx;
				regs->di = next->regs.di;
				regs->si = next->regs.si;
				regs->cx = next->regs.cx;
				regs->ip = next->regs.ip;
				regs->dx = next->regs.dx;
				regs->ax = next->regs.ax;
				regs->orig_ax = next->regs.ax;
				regs->sp = next->regs.sp;
				regs->bp = next->regs.bp;
				//cos_meas_event(COS_MEAS_BRAND_UC);
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

static void timer_interrupt(unsigned long data)
{
	BUG_ON(composite_thread == NULL);
	mod_timer(&timer, jiffies+1);

	if (!(cos_timer_brand_thd && cos_timer_brand_thd->upcall_threads)) {
		return;
	}

	host_attempt_brand(cos_timer_brand_thd);
	return;
}

static void register_timers(void)
{
	init_timer(&timer);
	timer.function = timer_interrupt;
	mod_timer(&timer, jiffies+2);
	
	return;
}

static void deregister_timers(void)
{
	cos_timer_brand_thd = NULL;    
	del_timer(&timer);

	return;
}

/***** end timer handling *****/

void thd_publish_data_page(struct thread *thd, vaddr_t page)
{
	unsigned int id = thd_get_id(thd);

	//assert(0 != id && 0 == (page & ~PAGE_MASK));

	//printk("cos: shared_region_pte is %p, page is %x.\n", shared_region_pte, page);
	/* _PAGE_PRESENT is not set */
	((pte_t*)shared_region_page)[id].pte_low = (vaddr_t)va_to_pa((void*)page) |
		(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED);

	return;
}

void switch_thread_data_page(int old_thd, int new_thd)
{
	assert(0 != old_thd && 0 != new_thd);

	/*
	 * Use shared_region_page here to avoid a cache miss going
	 * through a level of indirection for a pointer.
	 *
	 * unmap the current thread map in the new thread
	 */
	((pte_t*)shared_region_page)[old_thd].pte_low &= ~_PAGE_PRESENT;
	((pte_t*)shared_region_page)[new_thd].pte_low |= _PAGE_PRESENT;

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

	kern_data = pgtbl_vaddr_to_kaddr((paddr_t)va_to_pa(current->mm->pgd), (unsigned long)shared_data_page);
	modval  = (paddr_t)va_to_pa((void *)kern_data);
	userval = (paddr_t)va_to_pa((void *)pgtbl_vaddr_to_kaddr((paddr_t)va_to_pa(current->mm->pgd), 
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

/*
 * Opening the aed device signals the intended use of the Composite
 * operating system along side the currently executing Linux.  Thus,
 * when the fd is open, we must prepare the virtual address space for
 * COS use.
 */
static int aed_open(struct inode *inode, struct file *file)
{
	pte_t *pte = lookup_address_mm(current->mm, COS_INFO_REGION_ADDR);
	pgd_t *pgd;
	void* data_page;

	if (composite_thread != NULL || composite_union_mm != NULL) {
		printk("cos: Composite subsystem already used by %d.\n", composite_thread->pid);
		return -EBUSY;
	}

	composite_thread = current;
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
	shared_region_pte = (pte_t *)pgtbl_vaddr_to_kaddr((paddr_t)va_to_pa(current->mm->pgd), 
							  (unsigned long)shared_region_page);
	if (((unsigned long)shared_region_pte & ~PAGE_MASK) != 0) {
		printk("Allocated page for shared region not page aligned.\n");
		return -EFAULT;
	}
	memset(shared_region_pte, 0, PAGE_SIZE);

	/* hook in the data page */
	data_page = va_to_pa((void *)pgtbl_vaddr_to_kaddr((paddr_t)va_to_pa(current->mm->pgd), 
							   (unsigned long)shared_data_page));
	shared_region_pte[0].pte_low = (unsigned long)(data_page) |
		(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED);
	/* hook up the actual virtual memory pages to the pte
	 * protection mapping equivalent to PAGE_SHARED */
/*	for (i = 0 ; i < MAX_NUM_THREADS+1 ; i++) { 
		shared_region_pte[i].pte_low = (unsigned long)(__pa(pages_ptr+(PAGE_SIZE*i))) | 
			(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED);
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
	cos_init_memory();

	register_timers();
	cos_meas_init();
	cos_net_init();

	return 0;
}

//extern void event_print(void);

static int aed_release(struct inode *inode, struct file *file)
{
	pgd_t *pgd;
	struct thread *t;
	struct spd *s;
#ifdef FAULT_DEBUG
	int i, j, k;
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

		current_active_guest = NULL;
		/* Let another process create a asym environment */
		trusted_mm = NULL;

		remove_all_guest_mms();
		flush_all(current->mm->pgd);
	}

	t = thd_get_current();
	if (t) {
		s = thd_get_thd_spd(t);
		printk("cos: Halting Composite.  Current thread: %d in spd %d\n",
		       thd_get_id(t), spd_get_index(s));
	}

	deregister_timers();
	cos_net_finish();

	/* our garbage collection mechanism: all at once when the cos
	 * system control fd is closed */
//	thd_free(thd_get_current());
	thd_free_all();
 	thd_init();
	spd_free_all();
	ipc_init();
	cos_shutdown_memory();
	composite_thread = NULL;

	cos_meas_report();

	/* reset the address space to the original process */
	composite_union_mm->pgd = union_pgd;

	/* 
	 * free the shared region...
	 * FIXME: should also kill the actual pages of shared memory
	 */
	pgd = pgd_offset(composite_union_mm, COS_INFO_REGION_ADDR);
	memset(pgd, 0, sizeof(int));

	/* 
	 * Keep the mm_struct around till we have gotten rid of our
	 * cos-specific mappings.  This is required as in do_exit, mm
	 * is dropped before files and fs (thus current->mm should not
	 * be accessed from fd release procedures.)
	 */
	mmput(composite_union_mm);
	composite_union_mm = NULL;
	
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
				printk("cos: spd %d, thd %d @ addr %x @ time %lld, mpd flags %x (master %x) and w/ regs: \ncos:\t\t"
				       "eip %10x, esp %10x, eax %10x, ebx %10x, ecx %10x,\ncos:\t\t"
				       "edx %10x, edi %10x, esi %10x, ebp %10x,\n"
				       "cos:\t\tcs %10x, ss %10x, flags %10x\n",
				       fi->spdid, fi->thdid, 
				       (unsigned int)fi->addr, 
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
			}
		}
		event_print();
	}

	return 0;
}

/* 
 * Modules are vmalloc allocated, which means that their memory is
 * lazy faulted into page tables.  If the page fault handler is in one
 * of the un-faulted-in pages, then the machine will die (double
 * fault).  Thus, make sure the vmalloc regions are updated in all
 * page tables.
 */
/* static void update_vmalloc_regions(void) */
/* { */
/* 	struct task_struct *t; */
/* 	pgd_t *curr_pgd; */

/* 	BUG_ON(!current->mm); */
/* 	curr_pgd = current->mm->pgd; */

/* 	printk("curr pgd @ %p, cpy from %x to %x.  module code sample @ %p.\n",  */
/* 	       (void*)pa_to_va((void*)curr_pgd), MODULES_VADDR, MODULES_END, &page_fault_interposition); */

/* 	list_for_each_entry(t, &init_task.tasks, tasks) { */
/* 		struct mm_struct *amm = t->active_mm, *mm = t->mm; */
		
/* 		if (current->mm == amm || current->mm == mm) continue; */

/* 		if (amm) copy_pgtbl_range_nonzero((paddr_t)amm->pgd, (paddr_t)curr_pgd,  */
/* 						  MODULES_VADDR, MODULES_END-MODULES_VADDR); */
/* 		if (mm && mm != amm) copy_pgtbl_range_nonzero((paddr_t)mm->pgd, (paddr_t)curr_pgd,  */
/* 							      MODULES_VADDR, MODULES_END-MODULES_VADDR); */
/* 	} */
/* } */

static struct file_operations proc_aed_fops = {
	.owner          = THIS_MODULE, 
	.ioctl          = aed_ioctl, 
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
static int asym_exec_dom_init(void)
{
	printk("cos: Installing the hijack module.\n");
	/* pt_regs in this linux version has changed... */
	BUG_ON(sizeof(struct pt_regs) != (17*sizeof(long)));

	if (make_proc_aed())
		return -1;

//	update_vmalloc_regions();
	hw_int_init();
	hw_int_override_sysenter(sysenter_interposition_entry);
//	hw_int_override_pagefault(page_fault_interposition);
	hw_int_override_idt(0, div_fault_interposition, 0, 0);
	hw_int_override_idt(0xe9, state_inv_interposition, 0, 3);

	BUG_ON(offsetof(struct thread, regs) != 8);

	init_guest_mm_vect();
	trusted_mm = NULL;

	return 0;
}

static void asym_exec_dom_exit(void)
{
	hw_int_reset();
	remove_proc_entry("aed", NULL);

	return;
}

module_init(asym_exec_dom_init);
module_exit(asym_exec_dom_exit);

MODULE_AUTHOR("Gabriel Parmer");
MODULE_DESCRIPTION("Composite Operating System support module for coexistence with Linux");
MODULE_LICENSE("GPL");
