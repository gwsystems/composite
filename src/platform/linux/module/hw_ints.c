/**
 * Copyright 2010 by Gabriel Parmer and The George Washington University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#include <asm/desc.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include "../../../kernel/include/shared/cos_config.h"
#include "../../../kernel/include/asm_ipc_defs.h"
#include "../../../kernel/include/chal/cpuid.h"

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
struct cos_desc_struct {
	unsigned short address_low;
	unsigned long trash __attribute__((packed));
	unsigned short address_high;
} __attribute__ ((packed));

static inline int 
decode_desc_addr(struct cos_desc_struct *desc, unsigned long *ip, int *dpl, int *intr)
{
	/* printk(">>> Descriptor address %x, other crap %x:%x.\n", */
	/*        ((desc->address_high<<16) | desc->address_low), */
	/*        (unsigned int)desc->trash & 0xFFFF, (unsigned int)desc->trash >> 16); */
	*dpl  = (desc->trash >> 29) & 0x3;
	/* interrupts disabled? */
	*intr = desc->trash & 1;
	*ip = (unsigned long)((desc->address_high<<16) | desc->address_low);

	return 0;
}

static inline void 
cos_set_idt_entry(unsigned int n, unsigned dpl, unsigned ints_enabled, 
		  void *addr, struct cos_desc_struct *idt_table)
{
	gate_desc s;
	int gate = n;
	unsigned type = ints_enabled ? GATE_TASK : GATE_INTERRUPT;
	unsigned ist = 0;
	unsigned seg = __KERNEL_CS;

	pack_gate(&s, type, (unsigned long)addr, dpl, ist, seg);
	write_idt_entry((void*)idt_table, gate, &s);
}

extern void *cos_default_page_fault_handler;
void *cos_realloc_page_fault_handler;
extern void *cos_default_div_fault_handler;
extern void *cos_default_reg_save_handler;
extern void *cos_default_timer_handler;
#ifdef FPU_ENABLED
extern void *cos_default_fpu_not_available_handler;
#endif

/* 
 * This is really just a pain in the ass.  See 5-14 (spec Figure 5-1,
 * 5-2) in March 2006 version of Vol 3A of the intel docs (system
 * programming volume A).
 */
struct cos_desc_ptr {
	unsigned short idt_limit;
	unsigned long idt_base;
} __attribute__((packed));

struct decoded_idt_desc {
	void *handler;
	unsigned ints_enabled, dpl;
} default_idt_entry[256];
struct cos_desc_ptr default_idt_desc;
struct cos_desc_struct *default_idt;
struct cos_desc_struct saved_idt[256] __attribute__((aligned(PAGE_SIZE)));
int cos_idt_sz;

void *cos_default_sysenter_addr;

#include "cos_irq_vectors.h"

extern void ipi_handler(void);
extern void reg_save_interposition(void);

#ifndef LOCAL_TIMER_VECTOR
#define LOCAL_TIMER_VECTOR         0xef
#endif

void
hw_int_init(void)
{
	int se_addr, trash, i;

	/* This will initialize the entire structure */
	__asm__ __volatile__("sidt %0" : "=m" (default_idt_desc.idt_limit));
	default_idt = (struct cos_desc_struct *)default_idt_desc.idt_base;
	/* integer rounding here: */
	cos_idt_sz = default_idt_desc.idt_limit/sizeof(struct cos_desc_struct);

	/* Copy the IDT, and the descriptor */
	memcpy(saved_idt, default_idt, default_idt_desc.idt_limit);
	BUG_ON(memcmp(default_idt, (void*)saved_idt, default_idt_desc.idt_limit));
	//__asm__ __volatile__("lidt %0" :: "m" (default_idt_desc.idt_limit));
	printk("IDT at %p:%d, saved state at %p, both with %d entries\n", 
	       default_idt, default_idt_desc.idt_limit, saved_idt, cos_idt_sz);

	/* Save the entry instructions for each int/fault handler */
	for (i = 0 ; i < cos_idt_sz ; i++) {
		struct decoded_idt_desc *did = &default_idt_entry[i];
		decode_desc_addr(&default_idt[i], (unsigned long*)&did->handler, 
				 &did->dpl, &did->ints_enabled);

		/* Yuck...we should simply use the array of saved handlers instead */
		switch (i) {
		case 0:
			cos_default_div_fault_handler  = did->handler;
			break;
		case 14:
			cos_default_page_fault_handler = did->handler;
			break;
		case COS_REG_SAVE_VECTOR:
			cos_default_reg_save_handler   = did->handler;
			break;
		case LOCAL_TIMER_VECTOR:
			cos_default_timer_handler      = did->handler;
			break;
#ifdef FPU_ENABLED
                case 7:
                        cos_default_fpu_not_available_handler = did->handler;
                        break;
#endif
		};
	}

	/* save the sysenter instruction address */
	rdmsr(MSR_IA32_SYSENTER_EIP, se_addr, trash);
	cos_default_sysenter_addr = (void*)se_addr;
}

/* reset hardware entry points  */
void
hw_int_reset(void *tss)
{
	memcpy((void*)default_idt, saved_idt, default_idt_desc.idt_limit);
	wrmsr(MSR_IA32_SYSENTER_EIP, (int)cos_default_sysenter_addr, 0);
	/* Linux has esp points to TSS struct. */
	wrmsr(MSR_IA32_SYSENTER_ESP, (int)tss, 0);
}

void
cos_kern_stk_init(void)
{
	struct cos_cpu_local_info *cos_info;
	struct thread_info *linux_thread_info = (struct thread_info *)get_linux_thread_info();

	cos_info = cos_cpu_local_info();
	/* No Linux thread/process migration allowed. */
	cos_info->cpuid = linux_thread_info->cpu;
	/* Init the cached data */
	cos_info->invstk_top = 0;
	/* value to detect stack overflow */
	cos_info->overflow_check = 0xDEADBEEF;
}

void
hw_int_override_sysenter(void *handler, void *tss_end)
{
	struct tss_struct *tss;
	struct cos_cpu_local_info *cos_info = cos_cpu_local_info();
	void *sp0;

	/* Store the tss_end in cos info struct. */
	cos_info->orig_sysenter_esp = tss_end;
	tss = tss_end - sizeof(struct tss_struct);
	/* We only uses 1 page stack. No need to touch 2 pages. */
	sp0 = (void *)tss->x86_tss.sp0 - PAGE_SIZE;

	wrmsr(MSR_IA32_SYSENTER_EIP, (int)handler, 0);
	wrmsr(MSR_IA32_SYSENTER_ESP, (int)sp0, 0);
	/* Now we have sysenter_esp points to actual sp0. No need to
	 * touch TSS page on Composite path! */
}

void
hw_int_override_timer(void *handler)
{
	cos_set_idt_entry(LOCAL_TIMER_VECTOR, 0, 0, handler, default_idt);
}

void
hw_int_cos_ipi(void *handler)
{
	cos_set_idt_entry(COS_IPI_VECTOR, 0, 0, handler, default_idt);
}

extern unsigned int *pgtbl_module_to_vaddr(unsigned long addr);

/* 
 * The problem: Module code and data is vmalloced, which means it is
 * faulted in on-demand into address spaces.  Whenever a new address
 * space is created, the code must be faulted in as accessed.  If we
 * want to define the page fault handler in module code, we have a
 * problem -- the page fault handler faults in pages, but the page
 * holding the code for the page fault handler needs faulting in.
 * This situation will result in double faults and automatic reboots.
 * So if we want to redefine the page fault handler (which we do), it
 * must be in non-vmalloced memory.  We create a trampoline
 * (page_fault_interposition_tramp) in the module, and find out which
 * page in normal memory it corresponds to (via
 * pgtbl_module_to_vaddr).  We then fixup all addresses assuming the
 * code will be executed from this new address, and set it to be the
 * new page fault handler.  The code itself is tricky as we can't
 * access registers, and can't reference many addresses (as absolute
 * addresses into the module just revisit this problem).
 *
 * In the trampoline, we now don't only check if the fault is
 * kernel-caused (thus should not be passed on to cos), but we also
 * check if the fault is at the new page fault handler.  If so, we
 * jump to the default handler.  This is certainly a kludge, but is
 * necessary because init (pid = 1) was segfaulting for trying to
 * access that page.  Why the fault was a user-level fault, I have no
 * idea.  It would kill init, which would brick the system.
 */
void 
relocate_page_fault_handler(void *handler)
{
	unsigned int **cos_default_deref_addr, **cos_interpose_deref_addr, *pf_realloc;
	/* Symbols associated with the page fault handling code */
	extern void *cos_page_fault_page_tramp, *page_fault_interposition_tramp, 
		*cos_interpose_page_fault_handler_tramp, *cos_default_page_fault_handler_tramp;
	/* Symbols within the page fault handling code (literally in the code) */
	extern unsigned int *cos_post_default_deref_addr_tramp, *cos_post_interpose_deref_addr_tramp;

	int default_ptr_off, interpose_ptr_off, interpose_off;
	unsigned int p = (unsigned int)&cos_page_fault_page_tramp;

	default_ptr_off   = (unsigned int)&cos_default_page_fault_handler_tramp - p;
	interpose_ptr_off = (unsigned int)&cos_interpose_page_fault_handler_tramp - p;
	interpose_off     = (unsigned int)&page_fault_interposition_tramp - p;

	/* Find the address in the kernel's main memory, not module space */
	pf_realloc = pgtbl_module_to_vaddr((unsigned long)&cos_page_fault_page_tramp);

	/* Fix up internal pointers within that page fault code for the new page location */
	cos_default_deref_addr = ((&cos_post_default_deref_addr_tramp)-1);
	*cos_default_deref_addr = pf_realloc + default_ptr_off/sizeof(*pf_realloc);
	cos_interpose_deref_addr = ((&cos_post_interpose_deref_addr_tramp)-1);
	*cos_interpose_deref_addr = pf_realloc + interpose_ptr_off/sizeof(*pf_realloc);

	/* Find the new address of the handler */
	cos_realloc_page_fault_handler = pf_realloc + interpose_off/sizeof(*pf_realloc);

	//assert(default_idt_entry[14].handler);
	cos_default_page_fault_handler_tramp = default_idt_entry[14].handler;
	cos_interpose_page_fault_handler_tramp = handler;
}

int
hw_int_override_pagefault(void *handler)
{
	static int first = 0;
	
	if (first == 0) { /* the init core will call this function first. other cores will initialize after it. */
		first = 1;
		relocate_page_fault_handler(handler);
	}
	
	cos_set_idt_entry(14, default_idt_entry[14].dpl, default_idt_entry[14].ints_enabled, 
			  cos_realloc_page_fault_handler, default_idt);

	return 0;
}

int
hw_int_override_idt(int fault_num, void *handler, int ints_enabled, int dpl)
{
	if ((fault_num == 14) || (fault_num > cos_idt_sz)) return -1;

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
	cos_set_idt_entry(fault_num, dpl, ints_enabled, handler, default_idt);

	return 0;
}
