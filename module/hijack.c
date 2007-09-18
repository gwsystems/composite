/**
 * Hijack, or Asymmetric Execution Domains support for Linux
 * By Gabriel Parmer, gabep1@cs.bu.edu
 *
 * GPL V2 License (FIXME: copy and paste license here)
 */

#include <linux/module.h>
//#include <linux/config.h>
#include <linux/init.h>
#include <linux/sched.h>
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

#include "aed_ioctl.h"
#include "asym_exec_dom.h"

#include "include/spd.h"
#include "include/ipc.h"
#include "include/thread.h"

MODULE_LICENSE("GPL");
#define MODULE_NAME "asymmetric_execution_domain_support"

/* exported in kernel/asym_exec_domain.c */
extern void (*switch_to_executive)(void);
//extern long (*syscall_switch_exec_domain)(void *ptr);
extern struct mm_struct* (*asym_page_fault)(unsigned long address);

extern void asym_exec_dom_entry(void);
extern void page_fault_interposition(void);

extern void *default_page_fault_handler;
extern void *sysenter_addr;
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

struct task_struct *composite_thread;
struct mm_struct *composite_union_mm;

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
	init_MUTEX(&mm->context.sem);
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
	memset(pgd, 0, span*sizeof(int));

	pgd = pgd_offset(mm, COS_INFO_REGION_ADDR);
	memset(pgd, 0, sizeof(int));

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

/*
 * If you want fine-grained measurements of operation overheads for
 * profiling, uncomment this variable.
 * 
 * Not defining this does not make measurements have 0 effects on the
 * runtime when compiled out, unfortunately, as a variable is returned
 * from start measurement that will be in surrounding codes.
 */
//#define MICRO_MEASUREMENTS
#ifdef MICRO_MEASUREMENTS
#include "../include/kern_meas.h"

int meas_copy_trusted_to_mm, meas_write_regs_to_user, meas_restore_exec_regs;
int meas_get_guest_mm, meas_save_exec_regs, meas_restore_guest_regs, 
	meas_copynclr_exec_reg, meas_switch_pgtbls, meas_flush_exec;

static void register_measurements(void)
{
	meas_copy_trusted_to_mm = register_measurement_point("g->e:copy_trusted_to_mm");
	meas_write_regs_to_user = register_measurement_point("g->e:write_regs_to_user");
	meas_restore_exec_regs  = register_measurement_point("g->e:restore_exec_regs");
	meas_get_guest_mm       = register_measurement_point("e->g:get_guest_mm");
	meas_save_exec_regs     = register_measurement_point("e->g:save_exec_regs");
	meas_restore_guest_regs = register_measurement_point("e->g:restore_guest_regs");
	meas_copynclr_exec_reg  = register_measurement_point("e->g:copynclr_exec_reg");
	meas_switch_pgtbls      = register_measurement_point("e->g:switch_tbls");
	meas_flush_exec         = register_measurement_point("e->g:flush_exec ");
}

static void deregister_measurements(void)
{
	deregister_measurement_point(meas_copy_trusted_to_mm);
	deregister_measurement_point(meas_write_regs_to_user);
	deregister_measurement_point(meas_restore_exec_regs );
	deregister_measurement_point(meas_get_guest_mm);
	deregister_measurement_point(meas_save_exec_regs);
	deregister_measurement_point(meas_restore_guest_regs);
	deregister_measurement_point(meas_copynclr_exec_reg);
	deregister_measurement_point(meas_switch_pgtbls);
	deregister_measurement_point(meas_flush_exec);
}
#else 
#define register_measurements()
#define deregister_measurements()
#define start_measurement() 0 
#define stop_measurement(a,b)
#endif

/*
 * This is called from within exec and where we are expected to
 * associate the new mm made via exec with the parents notion of what
 * this address space is so that SWITCH_MM still works.
 *
 * This is kind of a crap idea, so I never implemented it.
 */
/* void exec_parent_vas_update(struct task_struct *parent, struct mm_struct *old_mm, struct mm_struct *new_mm) */
/* { */
/* 	struct file *file; */
/* 	//struct files_struct *files; */
	
/* 	if (!parent) { */
/* 		printk("cosParent of trusting child doesn't exist to change VAS pointer.\n"); */
/* 		return; */
/* 	} */

/* 	//files = parent->files; */
/* 	//spin_lock(&files->file_lock); */
	
/* 	/\*  */
/* 	 * FIXME: I should do the reference counting on the */
/* 	 * file, but this will only cause races when we are */
/* 	 * opening alot of address space and execing a lot at */
/* 	 * the same time...something I am not going to test */
/* 	 * now. */
/* 	 *\/ */
/* 	file = old_mm->trusted_executive; */
/* 	if (!file) BUG(); */
	
/* 	/\* A little error checking...this should never happen *\/ */
/* 	/\* */
/* 	if(file->f_op != &proc_mm_fops || file->private_data != old_mm) { */
/* 		spin_unlock(&files->file_lock); */
/* 		printk("cosConsistency problem between trusted and untrusted domains.\n"); */
/* 		force_sig(SIGKILL, current); */
/* 		force_sig(SIGKILL, parent); */
/* 		break; */
/* 	} */
/* 	*\/ */

/* 	/\* update the mapping between fd and address space *\/ */
/* 	file->private_data = new_mm; */
	
/* 	//spin_unlock(&files->file_lock); */
	
/* 	return; */
/* } */


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

	return (struct pt_regs*)((int)current->thread.esp0 - sizeof(struct pt_regs));

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
	 * syscalls for experiments using Composite.
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
	 */
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
#define HPAGE_SHIFT	22
#define HPAGE_SIZE	((1UL) << HPAGE_SHIFT)
#define HPAGE_MASK	(~(HPAGE_SIZE - 1))

static inline void pte_clrglobal(pte_t* pte)
{ 
	pte->pte_low &= ~_PAGE_GLOBAL; 
}

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

/*
 * The lower and upper address ranges to be set to not global.  Must
 * be 4M aligned on x86 32bit.
 */
static inline void set_trusted_as_nonglobal(struct mm_struct* mm, 
					    unsigned long lower_addr, 
					    unsigned long region_size)
{
	unsigned long curr_addr = lower_addr, 
		upper_addr = lower_addr + region_size;
	
	/* FIXME: this will break for superpages */
	while (curr_addr < upper_addr) {
		pte_t *pte;

		pte = lookup_address_mm(mm, curr_addr);
		
		if (pte) {
			pte_clrglobal(pte);
		}

		curr_addr += PAGE_SIZE;
	}
}

/* FIXME: change to clone_pgd_range */
static inline void copy_pgd_range(struct mm_struct *to_mm, struct mm_struct *from_mm,
				  unsigned long lower_addr, unsigned long size)
{
	pgd_t *tpgd = pgd_offset(to_mm, lower_addr);
	pgd_t *fpgd = pgd_offset(from_mm, lower_addr);
	unsigned int span = size>>HPAGE_SHIFT;

	if (!(pgd_val(*fpgd) & _PAGE_PRESENT)) {
		printk("cos: BUG: nothing to copy in mm %p's pgd @ %x.\n", 
		       from_mm, (unsigned int)lower_addr);
	}

	/* sizeof(pgd entry) is intended */
	memcpy(tpgd, fpgd, span*sizeof(unsigned int));
}

static inline void copy_trusted_to_mm(struct mm_struct *mm, struct mm_struct *executive_mm, 
				      unsigned long lower_addr, unsigned long region_size)
{
	pgd_t *pgd = pgd_offset(mm, lower_addr);
	pgd_t *epgd = pgd_offset(executive_mm, lower_addr);

	int span = region_size>>HPAGE_SHIFT;

#ifdef DEBUG
	if (!(pgd_val(*pgd_offset(executive_mm, (int)user_level_regs)) & _PAGE_PRESENT)) {
		printk("cos: BUG: copying trusted->current, the executive's "
		       "first page is not present in the pgd.\n");
		//return;
	}
#endif

	/*
	printkd("cos: pre-copying memory at %x: %x to location %x: %x.\n", 
		(unsigned int)(epgd), (unsigned int)pgd_val(*epgd), 
		(unsigned int)(pgd), (unsigned int)pgd_val(*pgd));
	*/

	/* sizeof(pgd entry) is intended */
	memcpy(pgd, epgd, span*sizeof(int));
	/*
	printkd("cos: post-copying memory at %x: %x to location %x: %x.\n", 
		(unsigned int)(epgd), (unsigned int)pgd_val(*epgd), 
		(unsigned int)(pgd), (unsigned int)pgd_val(*pgd));
	*/
#ifdef DEBUG
	if (!(pgd_val(*pgd_offset(mm, (int)user_level_regs)) & _PAGE_PRESENT)) {
		printk("cos: BUG: copying trusted->current, the guest's "
		       "first page is not present in the pgd.\n");
	}
#endif

	return;
}

static inline void copy_and_clear_trusted(struct mm_struct *guest_mm, struct mm_struct *executive_mm, 
					  unsigned long lower_addr, unsigned long region_size)
{
	pgd_t *pgd = pgd_offset(guest_mm, lower_addr);
	pgd_t *epgd = pgd_offset(executive_mm, lower_addr);
	int span = region_size>>HPAGE_SHIFT;

#ifdef DEBUG
	if (!(pgd_val(*epgd) & _PAGE_PRESENT)) {
		printk("cos: BUG: copying to trusted and zeroing, the executive's first "
		       "page is not present in the pgd.\n");
		return;
	}
#endif

	/*
	 * This next statement not essential when the pgd entry is
	 * assured to be loaded in the trusted mm.
	 *
	 * If were are executing in the guest page tables while in the
	 * executive (which we are currently), then we need to make
	 * any updates we have made to the trusted page tables.
	 */
	if (likely(current->mm == guest_mm)) {
		memcpy(epgd, pgd, span*sizeof(int));
	}
	memset(pgd, 0, span*sizeof(int));

	return;
}

#define flush_all(pgdir) load_cr3(pgdir)
#define my_load_cr3(pgdir) asm volatile("movl %0,%%cr3": :"r" (__pa(pgdir)))
#define flush_executive(pgdir) my_load_cr3(pgdir)

#ifdef DEBUG
void show_registers(struct pt_regs *regs)
{
	printkd("cos: EIP:    %04x:[<%08lx>]    EFLAGS: %08lx\n",
	        0xffff & regs->xcs, regs->eip,
		regs->eflags);
	//printk("cos: EIP is at %s\n", regs->eip);
	printkd("cos: eax: %08lx   ebx: %08lx   ecx: %08lx   edx: %08lx\n",
		regs->eax, regs->ebx, regs->ecx, regs->edx);
	printkd("cos: esi: %08lx   edi: %08lx   ebp: %08lx   esp: %08lx\n",
		regs->esi, regs->edi, regs->ebp, regs->esp);
	printkd("cos: ds: %04x   es: %04x   ss: %04x\n",
		regs->xds & 0xffff, regs->xes & 0xffff, regs->xss & 0xffff);
	printkd("cos: Process %s (pid: %d, threadinfo=%p task=%p)\n",
		current->comm, current->pid, current_thread_info(), current);
	return;
}
#else
#define show_registers(x)
#endif


/*
 * The mechanism for changing execution contexts from the truster to
 * the executive.  See syscall_virtualization in entry.S for the
 * context in which this is called.
 * 
 * NOTE: we cannot interrupt this method while the kernel is
 * non-preemptive.
 */
void module_switch_to_executive(void)
{
	struct thread_info *ti = current_thread_info();
	struct task_struct *cur = current;
	struct pt_regs *regs = get_user_regs();
	unsigned long long start;
	//unsigned long flags;

	/*
	 * Context switch from the truster to the executive:
	 * 1) switch back memory contexts
	 * 2) Set the task so that it can make kernel calls
	 * 3) copy the current regs to the executive's structures
	 * 4) copy the executive's structures from the trusted location
	 *    to be the current thread's regs.
	 */

	/* switching full mms
        if (cur->trusted_mm != cur->active_mm) {
		cur->active_mm = cur->trusted_mm;
		cur->mm = cur->trusted_mm;

		load_cr3(cur->mm->pgd);
		//global_flush_tlb();		
	}
	*/

	//rdtscll(start);
	start = start_measurement();
	
	//local_save_flags(&flags);
	
	copy_trusted_to_mm(cur->mm, trusted_mm, 
			   trusted_mem_limit, trusted_mem_size);

	stop_measurement(meas_copy_trusted_to_mm, start);

	clear_ti_thread_flag(ti, TIF_VIRTUAL_SYSCALL);
	
	//local_irq_restore(&flags);

	printkd("cos: module_switch_to_executive(start...)\nChild registers:\n");
	show_registers(regs);
	printkd("cos: \n");

	start = start_measurement();

	/* context switch back to the executive */
	if (write_regs_to_user(user_level_regs)) {
		printk("cos: aed: Whoops, error copying regs to user on switch to executive.\n");
		/* FIXME: this is a problem case that needs to be solved */
		return;
	}
	
	stop_measurement(meas_write_regs_to_user, start);

	start = start_measurement();
	memcpy(regs, &trusted_regs, sizeof(struct pt_regs));
	stop_measurement(meas_restore_exec_regs, start);

	printkd("cos: switching back from guest to executive in mm %p.\n", cur->mm);

	printkd("cos: Executive registers:\n");
	show_registers(regs);
	printkd("cos: module_switch_to_executive(...end).\n\n");

	/* 
	 * must return either 0 or return code (-errno) as this will
	 * return from the ioctl.
	 */
	regs->eax = 0;

	return;
}

#ifdef DEBUG
static inline void print_pte_info(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *off = pgd_offset(mm, addr);
	
	printkd("cos: Pte info for %p at addr %x is %p:%x.\n", 
		mm, (unsigned int)addr,
		off, (unsigned int)pgd_val(*off));
	
/*
	if (!lookup_address_mm(mm, addr)) {
		printk("cos: Cannot find pte within guest tables after mapping.\n");
	}
*/

	return;
}
#else
#define print_pte_info(a, b)
#endif

long module_switch_to_child(void* __user ptr)
{
	struct pt_regs  *uregs = get_user_regs();
	child_context_t *ct = (child_context_t*)ptr;//(child_context_t*)uregs->ebx;
	struct mm_struct *mm;
	struct mm_struct *old_mm = current->mm;
	/* horrendious names...refactor */
	/* FIXME: don't access ct till copied into kernel */
	struct pt_regs *regs = &ct->regs;
	unsigned long long start;

	/* FIXME: insert real error code here */
	if (ptr == NULL) return -1;

	start = start_measurement();

	mm = aed_get_mm(ct->procmm_fd);
	if (!mm) {
		printk("cos: aed: no appropriate mm found.\n");
		return -EFAULT;
	}

	stop_measurement(meas_get_guest_mm, start);

	printkd("cos: module_switch_to_child(start...)\nExecutive registers:\n");
	show_registers(uregs);
	printkd("\n");

	/* 
	 * Context switch from executive to truster:
	 * 1) copy the executive's regs to a safe position
	 * 2) copy the trusters regs from the executive to this thread's regs
	 * 3) set the flag so that truster can't do anything in kernel
	 * 4) switch mm structs
	 */
	
	start = start_measurement();

	/* save the executive's regs */
	memcpy(&trusted_regs, uregs, sizeof(struct pt_regs));
	user_level_regs = regs;

	stop_measurement(meas_save_exec_regs, start);

	start = start_measurement();

	/* copy the intended truster's regs to the current task's registers */
	if (save_user_regs_to_kern(regs, uregs)) {
		printk("cos: aed: Fault when copying regs to executive.\n");
		return -EFAULT;
	}

	stop_measurement(meas_restore_guest_regs, start);

	/* 
	 * Set the task such that it will switch back to the
	 * executive upon syscall
	 */
	set_ti_thread_flag(current_thread_info(), TIF_VIRTUAL_SYSCALL);
	//task_unlock(current);

	//print_pte_info(current->mm, (unsigned long)user_level_regs);

	start = start_measurement();
	/*
	 * Cannot access executive user-pages between the load of the
	 * cr3 and here or else they will get loaded into the tlb,
	 * negating protection. FIXME: this should be done with the
	 * page table lock taken for trusted_mm.
	 */
	copy_and_clear_trusted(mm, trusted_mm, 
			       trusted_mem_limit, trusted_mem_size);

	stop_measurement(meas_copynclr_exec_reg, start);

	/* 
	 * Fast path should be interposition and response, not
	 * switching page tables.
	 */
	if (unlikely(old_mm != mm)) {
		current->mm = mm;
		current->active_mm = mm;
		/* FIXME: remove this when we are not switching mms */
		//trusted_mm = old_mm;

		/* 
		 * There are two options here: 1) we are legitimately
		 * switching page tables, in which case the TLB must
		 * be (really) flushed, 2) the mms don't match up
		 * because we are in the trusted_mm due to a page
		 * fault, in which case we really don't want to flush
		 * the tlb, and instead update the
		 * current_active_guest variable and flush out the
		 * executive's pages.
		 */
		if (unlikely(mm != current_active_guest)) {
			start = start_measurement();

			printkd("cos: flushing all pages and ");
			flush_all(mm->pgd);
			if (unlikely(old_mm->context.ldt != mm->context.ldt)) {
				/* 
				 * If the guest or executive attempts to add
				 * ldt entries and change them, get angry...we
				 * don't support such rubbish.
				 */
				printk("cos: BUG: ldt is not the same between mm's.\n");
			}
			stop_measurement(meas_switch_pgtbls, start);

			current_active_guest = mm;
		} else {
			/* 
			 * ok, the correct page tables ARE active, so
			 * now we just need to flush out the
			 * executive.  
			 */
			start = start_measurement();
			
			flush_executive(mm->pgd);
			
			stop_measurement(meas_flush_exec, start);
		}
		//global_flush_tlb();
	} else {
		/* 
		 * Now that the pages are removed from the page table, remove
		 * from tlb.
		 */
		start = start_measurement();

		flush_executive(mm->pgd);

		stop_measurement(meas_flush_exec, start);
	}

	printkd("cos: switching mm's from %x to %x.\n",
	       (unsigned int)old_mm, (unsigned int)mm);

	//print_pte_info(current->mm, (unsigned long)user_level_regs);

	printkd("cos: Child registers (%x mm):\n", (unsigned int)current->mm);
	show_registers(/*uregs*/get_user_regs());
	printkd("cos: module_switch_to_child(...end)\n\n");

	/* set the eax of the child process: */
	return uregs->eax;
}

/*
 * Make it so that the executive region will not be copied when we
 * make guest address spaces.  This actually is probably obsolete as
 * we will not service any page faults in the executive region with
 * guest page tables, instead only trusted tables.
 *
 * This should be invoked when we want to close the mm of the executive.
 */
void prevent_executive_copying_on_fork(struct mm_struct *mm, 
				       unsigned long start, unsigned long size)
{
	struct vm_area_struct *mpnt;
	/*
	 * +1 on the end because vm_end is defined as one byte past
	 * the last memory address in the mapping.
	 */
	unsigned long end = start+size+1; 

	down_write(&mm->mmap_sem);
	flush_cache_mm(current->mm);

	if (!mm | !mm->mmap) {
		printk("cos: no mm to prevent copying for!\n");
		return;
	}

	for (mpnt = mm->mmap ; mpnt ; mpnt = mpnt->vm_next) {
		/* 
		 * If the vma is within the executive's range, then
		 * set it as DONTCOPY, so that when we copy the
		 * address space (like fork), it won't copy these
		 * pages.  They will only exist persistently in the
		 * original address space, not those of the children.
		 * They must be dynamically mapped in there.
		 */
		if (mpnt->vm_start >= start && mpnt->vm_end <= end) {
			mpnt->vm_flags |= VM_DONTCOPY;
			printkd("cos: Setting vmarea going from %x to %x to dont copy.\n",
				(unsigned int)mpnt->vm_start, (unsigned int)mpnt->vm_end);
		}
	}

	//flush_tlb_mm(current->mm);
	up_write(&mm->mmap_sem);

	return;
}

extern int copy_mm(unsigned long clone_flags, struct task_struct * tsk);

static int aed_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	switch(cmd) {
		/* 
		 * A task will want to be trusted when it creates a
		 * child process that will have mm_structs controlled
		 * by the parent, and will want to be untrusted
		 * otherwise.
		 */
	case AED_PROMOTE_EXECUTIVE:
	{
		/* 
		 * The executive should lie completely above the
		 * boundry and all applications should lie below.
		 */
		executive_mem_limit_t mem_boundry;

		/* if we already have a hijack environment, disallow
		 * this thread to promote. */
		if (trusted_mm) {
			/* FIXME: need a better return value here */
			return -EINVAL;
		}
		
		if (copy_from_user(&mem_boundry, (void*)arg, 
				   sizeof(executive_mem_limit_t))) {
			//printk("cos: ");
			return -EFAULT;
		}

		/* in kernel memory space */
		if (mem_boundry.lower + mem_boundry.size >= PAGE_OFFSET) {
			return -EINVAL;
		}

		/*
		 * We must align on a pgd page boundry, and if code is
		 * contained at mem_boundry, then we must round _down_
		 * to the nearest super-page (pgd page).  ie. round
		 * down to the nearest 4M.
		 */
		if (mem_boundry.lower % HPAGE_SIZE != 0) {
			mem_boundry.lower = ((mem_boundry.lower+HPAGE_SIZE-1)&
					     HPAGE_MASK)-HPAGE_SIZE;
		}
		/* 
		 * Now round up...
		 */
		if (mem_boundry.size % HPAGE_SIZE != 0) {
			mem_boundry.size =  (mem_boundry.size+HPAGE_SIZE-1)&HPAGE_MASK;
		}

		trusted_mem_limit = mem_boundry.lower;
		trusted_mem_size = mem_boundry.size;

		/*
		 * FIXME: going to assume that no vm_area_structs span
		 * across one of the 4M borders.  Should insert checks.
		 */
		set_trusted_as_nonglobal(current->mm, mem_boundry.lower, mem_boundry.size);

		trusted_mm = current->mm;
		set_ti_thread_flag(current_thread_info(), TIF_HIJACK_ENV);

		printk("cos: [Promoting to executive the range from %x of size %d]\n",
			(unsigned int)mem_boundry.lower, (unsigned int)mem_boundry.size);

		break;
	}
	/* depricated 
	case AED_TESTING:
		//struct thread_info *ti = current_thread_info();
		printk("cos: Setting _TIF_VIRTUAL_SYSCALL bit in thread structure.\n");
		set_ti_thread_flag(current_thread_info(), TIF_VIRTUAL_SYSCALL);

		break;
	*/

	/* DEPRICATED: old and probably broken interface */
	case AED_CTXT_SWITCH:
	{
		//printk("cos: Depricated interface, don't use.\n");
		ret = module_switch_to_child((void*)arg);

		return ret;

		break;
	}
	case AED_SWITCH_MM:
	{
		//struct mm_struct *old = current->mm;
		struct mm_struct *new = aed_get_mm((int)arg);

		if (!new) {
			printk("cos: Invalid address space descriptor.\n");
			ret = -EINVAL;
			break;
		}

		//atomic_inc(&new->mm_users);

		//printk("cos: about to copy trusted ");

		current->mm = new;
		current->active_mm = new;
		current_active_guest = new;

		copy_trusted_to_mm(current->mm, trusted_mm, 
				   trusted_mem_limit, trusted_mem_size);

		flush_all(new->pgd);

		/*if (unlikely(old->context.ldt != new->context.ldt)) {
			printk("cos: BUG: ldt is not the same between mms.\n");
		}*/
		//task_unlock(child);

		//printk("cos: switching mm contexts from %p to %p.\n", old, new);

		//mmput(old);

		break;
	}
	/* 
	 * Copy the current memory contents into a mm provided by
	 * proc_mm.  Not a fast path.
	 */
	case AED_COPY_MM:
	{
		int fd = (unsigned int)arg;
		struct mm_struct *mm = aed_get_mm(fd);
		/* 
		 * dummy empty task struct because kernel interfaces
		 * for copy_mm are a little daft.  If functions such
		 * as dup_mmap were available (exported), then we
		 * could avoid this hack, but they aren't.
		 */
		struct task_struct *dummy = NULL;

		if (/*IS_ERR(mm) || */!mm) {
			printk("cos: fd passed to copy to, %d, is not valid.\n", fd);
			ret = -1;
			goto free_dummy;

		}

		/*
		 * We only want one copy of the executive's pages
		 * floating around: This has to be done every time we
		 * copy a mm rather than once at executive promotion
		 * time as the mmappings of the executive could have
		 * changed since we first promoted it.
		 */
		prevent_executive_copying_on_fork(trusted_mm, trusted_mem_limit, 
						  trusted_mem_size); 

		dummy = kmalloc(sizeof(*dummy), GFP_KERNEL);
		if (!dummy) {
			ret = -ENOMEM;
			break;
		}

		memset(dummy, 0, sizeof(*dummy));

		/* Copy the current mm into a dummy task. */
		if ((ret = copy_mm(0, dummy)) != 0) {
			printk("cos: Could not copy mm, error %d.\n", ret);
			ret = -1;
			goto free_dummy;
		}

		/* if the mm_handle doesn't exist */
		if (aed_replace_mm(fd, dummy->mm)) {
			printk("cos: Attempted to replace an mm you don't own a handle to.\n");
			ret = -1;
			goto free_dummy;
		}

		dummy->mm = NULL;
		dummy->active_mm = NULL;

		printkd("cos: old mm %x replaced in fd %d with %x.\n",
			(unsigned int)mm, fd, (unsigned int)aed_get_mm(fd));

free_dummy:
		kfree(dummy);

		break;		
	}
	case AED_CREATE_MM:
	{
		int mm_handle = aed_allocate_mm();

		return mm_handle;
	}
	case AED_GET_REGSTATE:
	{
		struct pt_regs *regs = (struct pt_regs*)arg;

		ret = write_regs_to_user(regs);

		printkd("cos: getting regs w/i mm %x.\n", (unsigned int)current->mm);

		break;
	}
	case AED_EXECUTIVE_MMAP:
	{
		struct mmap_args args;

		if (copy_from_user(&args, (void*)arg, 
				   sizeof(struct mmap_args))) {
			return -EFAULT;
		}

		/* FIXME: implement! */
		
		ret = -EINVAL;
		break;
	}
	case AED_TEST:
	{
		unsigned long vals[2];
		if (copy_from_user(vals, (void*)arg, 
				   sizeof(unsigned long)*2)) {
			//printk("cos: ");
			return -EFAULT;
		}

		jmp_addr = vals[0];
		stub_addr = vals[1];
		asm ("movl %%cr3, %0\n" : "=r" (saved_cr3));
		break;
	}
	case AED_CREATE_SPD:
	{
		struct spd_info spd_info;
		struct spd *spd;

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

		/* This is a special case where the component should
		 * share the address space of the configuring process,
		 * thus have access to all component's memory.  This
		 * won't survive iterations, but for now it is a
		 * convenient mechanism so that I don't have to setup
		 * stubs and ipc to the configuration process
		 * itself. */
		if (spd_info.lowest_addr == 0) {
			spd->spd_info.pg_tbl = (phys_addr_t)(__pa(current->mm->pgd));
			spd->location.lowest_addr = 0;
		} else {
			/*
			 * Copy relevant page table entries from the
			 * master page tables into this component's
			 * tables.  This includes not only the
			 * component's memory entries, but also the
			 * shared region.
			 */
			struct mm_struct *mm;
			spd->local_mmaps = aed_allocate_mm();
			if (spd->local_mmaps < 0) {
				spd_free(spd);
				return spd->local_mmaps;
			}
			mm = aed_get_mm(spd->local_mmaps);

			spd->spd_info.pg_tbl = (phys_addr_t)(__pa(mm->pgd));
			spd->location.lowest_addr = spd_info.lowest_addr;
			spd->location.size = spd_info.size;
			copy_pgd_range(mm, current->mm, spd_info.lowest_addr, spd_info.size);
			copy_pgd_range(mm, current->mm, COS_INFO_REGION_ADDR, PGD_RANGE);
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
		struct thread *thd;
		struct spd *spd;

		if (copy_from_user(&thread_info, (void*)arg, 
				   sizeof(struct cos_thread_info))) {
			//printk("cos: Error copying thread_info from user.\n");
			return -EFAULT;
		}

		spd = spd_get_by_index(thread_info.spd_handle);
		if (!spd) {
			printk("Spd %d invalid for thread creation.\n", thread_info.spd_handle);
			return -EINVAL;
		}

		thd = thd_alloc(spd);
		if (!thd) {
			printk("Could not allocate thread.\n");
			return -ENOMEM;
		}

		thd_set_current(thd);

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

		return 0;
	}
	case AED_EMULATE_PREEMPT:
	{
		return 0;
	}
	default: 
		ret = -EINVAL;
	}

	return ret;
}


/*
 * Event system if signals have too much overhead (or if the current
 * bug is intrusive).
 */
/* int event_thread(void *arg) */
/* { */
/* 	wait_queue_t wait_var; */
/* 	struct task_struct *me = current; */
/* 	struct upcall_desc *ucdesc = (struct upcall_desc*)ucd; */
	
/* 	/\* Need to record the task struct for stack switcher lookups *\/ */
/* 	ucdesc->thread = me; */
	
/* 	/\* Disassociate from the parent process *\/ */
/* 	me->session = 1; */
/* 	me->pgrp = 1; */
/* 	me->tty = NULL; */
	
/* 	exit_mm(me); */

/* 	/\* Block all signals except SIGKILL so we can do_exit() on KILL */
/* 	 * - disabled this for apache in sandbox experiments */
/* 	 *   as apache expects to receive certain signals */
/* 	 spin_lock_irq(&me->sigmask_lock); */
/* 	 sigfillset(&me->blocked); */
/* 	 siginitsetinv(&me->blocked, sigmask(SIGKILL)); */
/* 	 recalc_sigpending(me); */
/* 	 spin_unlock_irq(&me->sigmask_lock); */
/* 	*\/ */
	
/* 	current->flags |= PF_UPCALLSAND; */
/* 	OpenSandbox(current); */
	
/* 	while(1) { */
/* 		/\* SIGKILL the only signal we accept for now *\/ */
/* 		/\* - not true when the signal mask is not modified above *\/ */
/* 		if(signal_pending(current)){ */
/* 			printk(KERN_DEBUG "quitting because of signal\n"); */
/* 			current->flags &= ~PF_UPCALLSAND; */
			
/* 			/\* Block all signals and exit *\/ */
/* 			spin_lock_irq(&me->sigmask_lock); */
/* 			sigfillset(&me->blocked); */
/* 			recalc_sigpending(me); */
/* 			spin_unlock_irq(&me->sigmask_lock); */
			
/* 			/\* MOD_DEC_USE_COUNT; *\/ */
/* 			_exit(0); */
/* 		} */
		
/* 		/\* Initialize the wait variable *\/ */
/* 		init_waitqueue_entry(&wait_var, me); */
		
/* 		add_wait_queue(&ucdesc->waitq, &wait_var); */
/* 		current->state = TASK_INTERRUPTIBLE; */
		
/* 		/\* no need to wait, events ready. *\/ */
/* 		if(ucdesc->event_count){ */
/* 			remove_wait_queue(&ucdesc->waitq, &wait_var); */
/* 			current->state = TASK_RUNNING; */
			
/* 			// deliver events */
/* 		} */
/* 		else { */
/* 			/\* wait for more events *\/ */
/* 			schedule(); */
/* 			remove_wait_queue(&ucdesc->waitq, &wait_var); */
/* 		} */
/* 	} */
/* } */

/* void create_event_thread(int priority) */
/* { */
/* 	return kernel_thread(event_thread, priority, CLONE_FS | CLONE_FILES)); */
/* } */

//	wait_queue_head_t waitq;     /* used to schedule event delivery in threaded upcalls */

/*
 * regs is really not a complete register set.  We should not access
 * anything other than the general purpose registers (anything under
 * and including orig_eax.)
 */
void mem_mapping_syscall(struct pt_regs *regs)
{
	
}

/*
 * The Linux provided descriptor structure is crap, probably due to
 * the intel spec for descriptors being crap:
 * 
 * struct desc_struct {
 *      unsigned long a, b;
 * };
 *
 * Again see docs to realize that the middle 4 bytes of the 8 bytes
 * are the address (what is relevant here).  Read that again, and see
 * the docs.  It is annoying.  All I want is the handler address.
 */
struct my_desc_struct {
	unsigned short address_low;
	unsigned long trash __attribute__((packed));
	unsigned short address_high;
} __attribute__ ((packed));

/* Begin excerpts from arch/i386/kernel/traps.c */
#define _set_gate(gate_addr,type,dpl,addr,seg) \
do { \
  int __d0, __d1; \
  __asm__ __volatile__ ("movw %%dx,%%ax\n\t" \
	"movw %4,%%dx\n\t" \
	"movl %%eax,%0\n\t" \
	"movl %%edx,%1" \
	:"=m" (*((long *) (gate_addr))), \
	 "=m" (*(1+(long *) (gate_addr))), "=&a" (__d0), "=&d" (__d1) \
	:"i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	 "3" ((char *) (addr)),"2" ((seg) << 16)); \
} while (0)

static void set_trap_gate(unsigned int n, void *addr, struct my_desc_struct *idt_table)
{
	_set_gate(idt_table+n,15,0,addr,__KERNEL_CS);
}
/* end traps.c rips */

static inline unsigned long decipher_descriptor_address(struct my_desc_struct *desc)
{
	return (desc->address_high<<16) | desc->address_low;
}

/*
 * Change the current page fault handler to point from where it is, to
 * our handler, and return the address of the old handler.
 */
static inline unsigned long change_page_fault_handler(void *new_handler)
{
	/* 
	 * This is really just a pain in the ass.  See 5-14 (spec
	 * Figure 5-1, 5-2) in March 2006 version of Vol 3A of the
	 * intel docs (system programming volume A).
	 */
	struct desc_ptr {
		unsigned short idt_limit;
		unsigned long idt_base;
	} __attribute__((packed));

	struct desc_ptr idt_descriptor;
	struct my_desc_struct *idt_table;
	unsigned long previous_fault_handler;

	/* This will initialize the entire structure */
	__asm__ __volatile__("sidt %0" : "=m" (idt_descriptor.idt_limit));

	idt_table = (struct my_desc_struct *)idt_descriptor.idt_base;

	previous_fault_handler = decipher_descriptor_address(&idt_table[14]);

	/*
	 * Now we have the previously active page fault address.  Set
	 * the address of the new handler in the idt.  This
	 * functionality ripped from Linux (see above).
	 *
	 * If you are getting a kernel panic here, your processor
	 * probably has the pentium f00f bug.  Check to see if
	 * CONFIG_X86_F00F_BUG is set in your .config file.  If so, a
	 * remedy needs to be found.  For instance, use the fixmap.h
	 * translation from include/asm-i386/fixmap.h
	 */
	set_trap_gate(14, new_handler, idt_table);

	return previous_fault_handler;
}

//#define DEBUG_PAGE_FAULTS
/*
 * Called from page_fault_interposition in kern_entry.S.  Interrupts
 * disabled...dont block!  Can assume current_active_guest is accessed
 * in a critical section due to this.
 */
static int legal_faults = 0, illegal_faults = 0;
void main_page_fault_interposition(void)
{
	unsigned long fault_addr;
	int virt_sys;
//	pte_t *pte;

	__asm__("movl %%cr2,%0":"=r" (fault_addr));

	if (composite_thread == current) {
		/* FIXME: we will really have to check for all spds in
		 * the composite_spd, rather than just casting to a
		 * single spd, but for now... */
		struct spd *curr_spd = (struct spd*)thd_get_current_spd();
		unsigned long low = curr_spd->location.lowest_addr;

		if (fault_addr < low ||
		    fault_addr >= low + curr_spd->location.size) {
			/* search through the vmas to find out if this
			 * is a legal reference given linux's rules.
			 * If so, continue. If not setup fault handler
			 * by saving register state into fault handler
			 * and running that thread.  return specific
			 * value from this function not allowing the
			 * linux pf handler to run. */

			//pte = lookup_address_mm(current->mm, fault_addr);
			/* send fault to fault handler */
			illegal_faults++;
		} else {
			/* see comment above in "if"...same error behavior */
			legal_faults++;
		}
	}

	
	/* normal process page fault */
	if (!test_thread_flag(TIF_HIJACK_ENV))
		return;

	virt_sys = test_thread_flag(TIF_VIRTUAL_SYSCALL);

	/*
	 * If in the executive, we page fault, and need to use
	 * the executive's (trusted) page translations.
	 */
	if (!virt_sys &&
	    fault_addr >= trusted_mem_limit &&
	    fault_addr < trusted_mem_limit+trusted_mem_size) {
		/* locking done in do_page_fault, not here.
		 * Interrupts disabled so no problem on uni, but FIXME
		 * need spinlock for multi.  */
		current->mm = trusted_mm;
		current->active_mm = trusted_mm;
#ifdef DEBUG_PAGE_FAULTS
		// printk is actually freezing the machine...why!!!???
		printk("cos: fault in executive @ %x.\n", (unsigned int)fault_addr);
#endif
	}
	/*
	 * If otherwise we fault in the guest regions, or are not in
	 * the executive, make sure we are using guest translation
	 * tables.  (We might not be if we changed page tables due to
	 * the previous conditional on a previous page fault.)
	 */
	else /*if (!(fault_addr >= trusted_mem_limit &&
		   fault_addr < trusted_mem_limit+trusted_mem_size) &&
		   current->mm == trusted_mm)*/ {
#ifdef DEBUG_PAGE_FAULTS
		printk("cos: fault in guest (%x->%x) @ %x.\n", 
		       (unsigned int)current->mm, (unsigned int)current_active_guest, 
		       (unsigned int)fault_addr);
#endif

		if (current_active_guest) {
			current->mm = current_active_guest;
			current->active_mm = current_active_guest;
		} /* else -> fault implied */
	}
		
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

unsigned long *data_pages;
void switch_thread_data_page(int old_thd, int new_thd)
{
	/* unmap the current thread */
	shared_region_pte[old_thd+1].pte_low &= ~_PAGE_PRESENT;
	/* map in the new thread */
	shared_region_pte[new_thd+1].pte_low |= _PAGE_PRESENT;

	return;
}

#define DATA_PAGE_ORDER 3 /* log(MAX_NUM_THREADS+1) */
/*
 * Opening the aed device signals the intended use of the Composite
 * operating system along side the currently executing Linux.  Thus,
 * when the fd is open, we must prepare the virtual address space for
 * COS use.
 */
static int aed_open(struct inode *inode, struct file *file)
{
	pte_t *pte = lookup_address_mm(current->mm, COS_INFO_REGION_ADDR);
	unsigned long pages, pte_page, *pages_ptr, *region_ptr;
	pgd_t *pgd;
	int i;

	if (composite_thread != NULL || composite_union_mm != NULL) {
		printk("cos: Composite subsystem already used by %d.\n", composite_thread->pid);
		return -EBUSY;
	}

	composite_thread = current;
	composite_union_mm = get_task_mm(current);
	union_pgd = composite_union_mm->pgd;

	if (pte != NULL) {
		printk("cos: address range for info region @ %x already used.\n",
		       (unsigned int)COS_INFO_REGION_ADDR);
		return -ENOMEM;
	}

	/* 
	 * FIXME: memory leaks when returning;
	 */

	/*
	 * The following breaks with linux conventions regarding
	 * virtual address space layout, rmappings, etc...  This is
	 * necessary to support Composite as a first-class
	 * abstraction.  It also assumes x86 with 2 level page tables
	 * (i.e. no pud/pmd due to pae, etc.).
	 */
	pte_page = __get_free_pages(GFP_KERNEL, 0);
	if (pte_page == 0) {
		return -ENOMEM;
	}

	pages = __get_free_pages(GFP_KERNEL, DATA_PAGE_ORDER);
	if (pages == 0) {
		return -ENOMEM;
	}
	data_pages = pages_ptr = (unsigned long *)pages;

	/* record the virtual address of the shared pte */
	shared_region_pte = (pte_t *)pte_page;
	if (((unsigned long)shared_region_pte & ~PAGE_MASK) != 0) {
		printk("Allocated page for shared region not page aligned.\n");
		return -EFAULT;
	}
	memset(shared_region_pte, 0, PAGE_SIZE);

	/* hook up the actual virtual memory pages to the pte
	 * protection mapping equivalent to PAGE_SHARED */
	for (i = 0 ; i < MAX_NUM_THREADS+1 ; i++) { /* +1 for the info page */
		shared_region_pte[i].pte_low = (unsigned long)(__pa(pages_ptr+(PAGE_SIZE*i))) | 
			(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED);
	}

	/* Where in the page directory should the pte go? */
	pgd = pgd_offset(current->mm, COS_INFO_REGION_ADDR);

	if (pgd_none(*pgd)) {
		printk("Could not get pgd_offset.\n");
		return -EFAULT;
	}

	/* hook up the pte to the pgd */
	pgd->pgd = (unsigned long)(__pa(shared_region_pte)) | _PAGE_TABLE;

#define MAGIC_VAL_TEST 0xdeadbeef

	/* test the mapping */
	*pages_ptr = MAGIC_VAL_TEST;
	region_ptr = (unsigned long *)COS_INFO_REGION_ADDR;
	if (*region_ptr != MAGIC_VAL_TEST) {
		printk("cos: Mapping of the cos shared region didn't work.\n");
		return -EFAULT;
	} else {
		printk("cos: Mapping of shared region worked: %x.\n", 
		       (unsigned int)*region_ptr);
	}

	return 0;
}

static int aed_release(struct inode *inode, struct file *file)
{
	pgd_t *pgd;

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

	/* our garbage collection mechanism: all at once when the cos
	 * system control fd is closed */
	thd_init();
	spd_free_all();
	ipc_init();
	composite_thread = NULL;

	/* reset the address space to the original process */
	composite_union_mm->pgd = union_pgd;

	/* 
	 * free the shared region...
	 * FIXME: should also kill the actual pages of shared memory
	 */
	pgd = pgd_offset(composite_union_mm, COS_INFO_REGION_ADDR);
	memset(pgd, 0, sizeof(int));

	__free_pages(virt_to_page(shared_region_pte), 0);
	__free_pages(virt_to_page(data_pages), DATA_PAGE_ORDER);

	/* 
	 * Keep the mm_struct around till we have gotten rid of our
	 * cos-specific mappings.  This is required as in do_exit, mm
	 * is dropped before files and fs (thus current->mm should not
	 * be accessed from fd release procedures.)
	 */
	mmput(composite_union_mm);
	composite_union_mm = NULL;
	
	printk("cos: %d illegal page faults, %d legal.\n", 
	       illegal_faults, legal_faults);

	return 0;
}

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

	ent = create_proc_entry("aed", 0222, &proc_root);
	if(ent == NULL){
		printk("cos: make_proc_aed : Failed to register /proc/aed\n");
		return -1;
	}
	ent->proc_fops = &proc_aed_fops;
	ent->owner = THIS_MODULE;

	return 0;
}

static int asym_exec_dom_init(void)
{
	int trash;

	printk("cos: Installing the asymmetric execution domains module.\n");

	if (make_proc_aed())
		return -1;

	rdmsr(MSR_IA32_SYSENTER_EIP, (int)sysenter_addr, trash);
	wrmsr(MSR_IA32_SYSENTER_EIP, (int)asym_exec_dom_entry, 0);
	printk("cos: Saving sysenter msr (%p) and activating %p.\n", 
	       sysenter_addr, asym_exec_dom_entry);
	default_page_fault_handler = (void*)change_page_fault_handler(page_fault_interposition);
	printk("cos: Saving page fault handler (%p) and activating %p.\n", 
	       default_page_fault_handler, page_fault_interposition);

	//switch_to_executive = module_switch_to_executive;
	//asym_page_fault = module_page_fault;

	init_guest_mm_vect();
	trusted_mm = NULL;

	thd_init();
	spd_init();
	ipc_init();

	/* init measurements */
	register_measurements();

	return 0;
}

static void asym_exec_dom_exit(void)
{
	remove_proc_entry("aed", &proc_root);

	printk("cos: Resetting sysenter wsr to %p.\n", sysenter_addr);
	wrmsr(MSR_IA32_SYSENTER_EIP, (int)sysenter_addr, 0);
	printk("cos: Resetting page fault handler to %p.\n", default_page_fault_handler);
	change_page_fault_handler(default_page_fault_handler);

	deregister_measurements();
	
	printk("cos: Asymmetric execution domains module removed.\n");
}

module_init(asym_exec_dom_init);
module_exit(asym_exec_dom_exit);

MODULE_AUTHOR("Gabriel Parmer");
MODULE_DESCRIPTION("Composite Operating System support module for coexistence with Linux");
MODULE_LICENSE("GPL");
