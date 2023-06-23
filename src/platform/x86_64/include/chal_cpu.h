#ifndef CHAL_CPU_H
#define CHAL_CPU_H

#include <assert.h>
#include <fpu.h>
#include <chal_asm_inc.h>
#include <tss.h>
#include <arch_consts.h>

typedef enum {
	CR0_PE    = 1 << 0,  /* Protected Mode Enable */
	CR0_MP    = 1 << 1,  /* Monitor co-processor */
	CR0_EM    = 1 << 2,  /* Emulation x87 FPU */
	CR0_TS    = 1 << 3,  /* Task switched */
	CR0_ET    = 1 << 4,  /* Extension type */
	CR0_NE    = 1 << 5,  /* Numeric error */
	CR0_WP    = 1 << 16, /* Write protect */
	CR0_AM    = 1 << 18, /* Alignment mask */
	CR0_NW    = 1 << 29, /* Not-write through */
	CR0_CD    = 1 << 30, /* Cache disable */
	CR0_PG    = 1 << 31  /* Paging */
} cr0_flags_t;

typedef enum {
	CR4_TSD        = 1 << 2,  /* time stamp (rdtsc) access at user-level disabled */
	CR4_PSE        = 1 << 4,  /* page size extensions (superpages) */
	CR4_PGE        = 1 << 7,  /* page global bit enabled */
	CR4_PCE        = 1 << 8,  /* user-level access to performance counters enabled (rdpmc) */
	CR4_OSFXSR     = 1 << 9,  /* if set, enable SSE instructions and fast FPU save & restore, or using SSE instructions will cause #UD */
	CR4_OSXMMEXCPT = 1 << 10, /* Operating System Support for Unmasked SIMD Floating-Point Exceptions */
	CR4_FSGSBASE   = 1 << 16, /* user level fs/gs access permission bit */
	CR4_PCIDE      = 1 << 17, /* Process Context Identifiers Enable*/
	CR4_OSXSAVE    = 1 << 18, /* XSAVE and Processor Extended States Enable */
	CR4_SMEP       = 1 << 20, /* Supervisor Mode Execution Protection Enable */
	CR4_SMAP       = 1 << 21, /* Supervisor Mode Access Protection Enable */
	CR4_PKE        = 1 << 22  /* MPK Support */
} cr4_flags_t;

typedef enum {
	XCR0    = 0,  /* XCR0 register */
} xcr_regs_t;

typedef enum {
	XCR0_x87      = 1 << 0,  /* X87(must be 1) */
	XCR0_SSE      = 1 << 1,  /* SSE enable */
	XCR0_AVX      = 1 << 2,  /* AVX enable */
} xcr0_flags_t;

static inline word_t
chal_cpu_cr0_get(void)
{
	word_t config;
	asm("mov %%cr0, %0" : "=r"(config));

	return config;
}

static inline void
chal_cpu_cr0_set(word_t config)
{
	asm("mov %0, %%cr0" : : "r"(config));
}

static inline unsigned long
chal_cpu_cr4_get(void)
{
	unsigned long config;
#if defined(__x86_64__)
	asm("movq %%cr4, %0" : "=r"(config));
#elif defined(__i386__)
	asm("movl %%cr4, %0" : "=r"(config));
#endif

	return config;
}

static inline void
chal_cpu_cr4_set(cr4_flags_t flags)
{
	unsigned long config = chal_cpu_cr4_get();
	config |= (unsigned long)flags;
#if defined(__x86_64__)
	asm("movq %0, %%cr4" : : "r"(config));
#elif defined(__i386__)
	asm("movl %0, %%cr4" : : "r"(config));
#endif
}

static inline u64_t
chal_cpu_xgetbv(u32_t xcr_n)
{
	u32_t low, high;
	u64_t ret;

	asm volatile(
		"xgetbv\n\t"
		:"=a"(low),"=d"(high) : "c"(xcr_n));

	ret = ((u64_t)high << 32) | low;
	return ret;
}

static inline void
chal_cpu_xsetbv(u32_t xcr_n, u64_t config)
{
	u32_t low, high;
	low  = (u32_t)config;
	high = config >> 32;

	asm volatile(
		"xsetbv\n\t" \
		::"a"(low), "d"(high), "c"(xcr_n));
}

static inline void
chal_cpu_eflags_init(void)
{
	unsigned long val;

#if defined(__x86_64__)
	asm volatile("pushfq ; popq %0" : "=r"(val));
	val |= 3 << 12; /* iopl */
	asm volatile("pushq %0 ; popfq" : : "r"(val));
#elif defined(__i386__)
	asm volatile("pushf ; popl %0" : "=r"(val));
	val |= 3 << 12; /* iopl */
	asm volatile("pushl %0 ; popf" : : "r"(val));
#endif

}

static inline void
chal_cpu_pgtbl_activate(uword_t pgtbl)
{
#if defined(__x86_64__)
	asm volatile("movq %0, %%cr3" : : "r"(pgtbl));
#elif defined(__i386__)
	asm volatile("mov %0, %%cr3" : : "r"(pgtbl));
#endif
}

#define IA32_SYSENTER_CS  0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176
#define MSR_PLATFORM_INFO 0x000000ce
#define MSR_APIC_BASE     0x1b
#define MSR_TSC_AUX       0xc0000103

#if defined(__x86_64__)
#define MSR_IA32_EFER		0xC0000080
#define MSR_STAR 		0xC0000081
#define MSR_LSTAR 		0xC0000082
#define MSR_SFMASK 		0xC0000084

#define MSR_FSBASE		0xC0000100
#define MSR_USER_GSBASE 	0xC0000101
#define MSR_KERNEL_GSBASE 	0xC0000102
#endif

extern void sysenter_entry(void);

static inline void
writemsr(u32_t reg, u32_t low, u32_t high)
{
	__asm__("wrmsr" : : "c"(reg), "a"(low), "d"(high));
}

static inline void
readmsr(u32_t reg, u32_t *low, u32_t *high)
{
	__asm__("rdmsr" : "=a"(*low), "=d"(*high) : "c"(reg));
}

static inline void
chal_cpuid(u32_t *a, u32_t *b, u32_t *c, u32_t *d)
{
	asm volatile("cpuid" : "+a"(*a), "+b"(*b), "+c"(*c), "+d"(*d));
}

static inline void
chal_flush_cache(void)
{
	asm volatile("wbinvd" : : : "memory");
}

static inline void
chal_cpu_coreid_set(u32_t coreid)
{
	writemsr(MSR_TSC_AUX, coreid, 0x0u);
}

/**
 * Write byte to specific port
 */
static inline void
outb(u16_t port, u8_t value)
{
	__asm__ __volatile__("outb %1, %0" : : "dN"(port), "a"(value));
}

/**
 * Read byte from port
 */
static inline u8_t
inb(u16_t port)
{
	u8_t ret;

	__asm__ __volatile__("inb %1, %0" : "=a"(ret) : "dN"(port));

	return ret;
}

static void
ack_irq(int n)
{
	if (n >= 40) outb(0xA0, 0x20); /* Send reset signal to slave */
	outb(0x20, 0x20);
}

static inline void *
chal_pa2va(paddr_t address)
{
	return (void *)(address + COS_MEM_KERN_START_VA);
}

static inline paddr_t
chal_va2pa(void *address)
{
	return (paddr_t)((unsigned long)address - COS_MEM_KERN_START_VA);
}

#define PGTBL_PAGEIDX_SHIFT (12)
#define PGTBL_FRAME_BITS (32 - PGTBL_PAGEIDX_SHIFT)

#define PGTBL_ENTRY_ADDR_MASK 0xfffffffffffff000
#define PGTBL_DEPTH 4
#define PGTBL_ENTRY_ORDER 9
#define PGTBL_FLAG_MASK 0xf800000000000fff
#define PGTBL_FRAME_MASK (~PGTBL_FLAG_MASK)
#define NUM_ASID_BITS (12)
#define NUM_ASID_MAX ((1 << NUM_ASID_BITS) - 1)
#define PGTBL_ASID_MASK (0xfff)
#define CR3_NO_FLUSH (1ul << 63)

#define PGTBL_ENTRY (1 << PGTBL_ENTRY_ORDER)
#define SUPER_PAGE_FLAG_MASK  (0x3FFFFF)
#define SUPER_PAGE_PTE_MASK   (0x3FF000)

/* FIXME:find a better way to do this */
#define EXTRACT_SUB_PAGE(super) ((super) & SUPER_PAGE_PTE_MASK)

/* Page table related prototypes & structs */
/* make it an opaque type...not to be touched */
typedef struct pgtbl *pgtbl_t;

struct pgtbl_info {
	pgtbl_t       pgtbl;
	prot_domain_t protdom;
} __attribute__((packed));

/* identical to the capability structure */
struct cap_pgtbl {
	struct cap_header h;
	u32_t             refcnt_flags; /* includes refcnt and flags */
	pgtbl_t           pgtbl;
	u32_t             lvl;       /* what level are the pgtbl nodes at? */
	struct cap_pgtbl *parent;    /* if !null, points to parent cap */
	u64_t             frozen_ts; /* timestamp when frozen is set. */
} __attribute__((packed));

// #define MPK_ENABLE 1

#ifdef MPK_ENABLE
static inline void
wrpkru(u32_t pkru)
{
	asm volatile (
		"xor %%rcx, %%rcx\n\t"
		"xor %%rdx, %%rdx\n\t"
		"mov %0,    %%eax\n\t"
		"wrpkru\n\t"
		:
		: "r" (pkru)
		: "eax", "rcx", "rdx"
	);
}

static inline u32_t
rdpkru(void)
{
	u32_t pkru;

	asm volatile(
		"xor %%rcx, %%rcx\n\t"
		"xor %%rdx, %%rdx\n\t"
		"rdpkru"
		: "=a" (pkru)
		:
		: "rcx", "rdx"
	);

	return pkru;
}

static inline u32_t
pkru_state(prot_domain_t protdom)
{
	u16_t mpk_key = PROTDOM_MPK_KEY(protdom);
	return ~(0b11 << (2 * mpk_key)) & ~0b11;
}

static inline void
chal_protdom_write(prot_domain_t protdom)
{
	/* we only update asid on pagetable switch */
	wrpkru(pkru_state(protdom));
}

static inline prot_domain_t
chal_protdom_read(void)
{
	unsigned long cr3;
	u16_t asid, mpk_key;

	u32_t pkru = rdpkru();
	assert(pkru);
	/* inverse of `pkru_state` */
	mpk_key = (32 - __builtin_clz(~pkru)) / 2 - 1;

	asm volatile("mov %%cr3, %0" : "=r"(cr3) : :);
	asid = (u16_t)(cr3 & PGTBL_ASID_MASK);

	return PROTDOM_INIT(asid, mpk_key);
}
#else /* !MPK_ENABLE */
static inline void wrpkru(u32_t pkru) {}
static inline u32_t rdpkru(void) { return 0; }
static inline u32_t pkru_state(prot_domain_t protdom) { return 0; }
static inline void chal_protdom_write(prot_domain_t protdom) {}
static inline prot_domain_t chal_protdom_read(void) { return 0; }
#endif /* MPK_ENABLE */

struct cpu_tlb_asid_map {
	pgtbl_t mapped_pt[NUM_ASID_MAX];
} CACHE_ALIGNED;

extern struct cpu_tlb_asid_map tlb_asid_map[NUM_CPU];

static inline pgtbl_t
chal_cached_pt_curr(prot_domain_t protdom)
{
	u16_t asid = PROTDOM_ASID(protdom);
	return tlb_asid_map[get_cpuid()].mapped_pt[asid];
}

static inline void
chal_cached_pt_update(pgtbl_t pt, prot_domain_t protdom)
{
	u16_t asid = PROTDOM_ASID(protdom);
	tlb_asid_map[get_cpuid()].mapped_pt[asid] = pt;
}

/* Update the page table */
static inline void
chal_pgtbl_update(struct pgtbl_info *pt)
{
	u16_t asid = PROTDOM_ASID(pt->protdom);

	/* lowest 12 bits is the context identifier */
	unsigned long cr3 = (unsigned long)pt->pgtbl | asid;

	/* fastpath: don't need to invalidate tlb entries; otherwise flush tlb on switch */
	if (likely(chal_cached_pt_curr(asid) == pt->pgtbl)) {
		cr3 |= CR3_NO_FLUSH;
	} else {
		chal_cached_pt_update(pt->pgtbl, asid);
	}

	asm volatile("mov %0, %%cr3" : : "r"(cr3));
}

/* Check current page table */
static inline pgtbl_t
chal_pgtbl_read(void)
{
	unsigned long pt;

	asm volatile("mov %%cr3, %0" : "=r"(pt) : :);

	return (pgtbl_t)(pt & PGTBL_ENTRY_ADDR_MASK);
}

static void
chal_cpu_init(void)
{
	unsigned long cr4 = chal_cpu_cr4_get();
	coreid_t cpu_id = coreid();

#if defined(__x86_64__)
	u32_t low = 0, high = 0;
	u64_t xcr0_config = 0;
	u32_t a = 0, b = 0, c = 0, d = 0;
	word_t cr0;

	a = 0x07;
	c = 0;
	chal_cpuid(&a, &b, &c, &d);
	if (c & (1 << 3)) {
		cr4 |= CR4_PKE;
		printk("CPU supports MPK: enabling.\n");
#ifndef MPK_ENABLE
		printk("ERROR: CPU supports MPK, but not enabled in build system. Please set MPK_ENABLE.\n");
		assert(0);
#endif
	} else {
		printk("CPU does NOT support MPK: not enabling.\n");
#ifdef MPK_ENABLE
		printk("ERROR: MPK not supported by hardware, but enabled in build system. Please unset MPK_ENABLE.");
		assert(0);
#endif
	}

	/* CR4_OSXSAVE has to be set to enable xgetbv/xsetbv */
	chal_cpu_cr4_set(cr4 | CR4_PSE | CR4_PGE | CR4_OSXSAVE | CR4_PCIDE);

	/* I'm not sure this is the best spot for this */
	assert(sizeof(struct ulk_invstk) == ULK_INVSTK_SZ);

	/* Check if the CPU support XSAVE and AVX */
	a = 0x01;
	chal_cpuid(&a, &b, &c, &d);
	/* bit 26 is XSAVE, bit 28 is AVX */
	assert((c & (1 << 26)) && (c & (1 << 28)));
	/* Check if SSE3 and SSE4 is supported */
	assert((c & (1 << 0)) && (c & (1 << 9)) && (c & (1 << 19)) && (c & (1 << 20)));
	/* Check if AVX2 is supported */
	a = 0x07;
	c = 0;
	chal_cpuid(&a, &b, &c, &d);
	assert(b & (1 < 5));
	printk("The CPU supports SSE3, SSE4, AVX, AVX2 and XSAVE\n");

	/* Check if the CPU suppor XSAVEOPT, XSAVEC and XSAVES instructions*/
	a = 0x0d;
	c = 1;
	chal_cpuid(&a, &b, &c, &d);
	assert((a & (1 << 0)) && (a & (1 << 1)) && (a & (1 << 3)));
	printk("The CPU supports XSAVEOPT, XSAVEC and XSAVES instructions\n");

	/* Get the maximum size of XSAVE area of available XCR0 features */
	a = 0x0d;
	c = 0;
	chal_cpuid(&a, &b, &c, &d);
	assert(c > 0);
	printk("The CPU maximum XSAVE area is: %u\n", c);

	/* Check the AVX state component offset from the beginning of XSAVE Area*/
	a = 0x0d;
	c = 2;
	chal_cpuid(&a, &b, &c, &d);
	printk("The AVX area offset is: %u\n", b);

	/* Now enable SSE and AVX in XCR0, so that XSAVE features can be used */

	/* 1. Enable SSE */
	cr0  = chal_cpu_cr0_get();
	cr0 &= ~((word_t)(CR0_EM)); /* clear EM bit*/
	cr0 |= (word_t)(CR0_MP);    /* set MP bit */
	chal_cpu_cr0_set(cr0);
	chal_cpu_cr4_set(CR4_OSFXSR);

	/* 2. Enable AVX */
	xcr0_config = chal_cpu_xgetbv(XCR0);
	xcr0_config |= XCR0_x87 | XCR0_SSE | XCR0_AVX;
	chal_cpu_xsetbv(XCR0, xcr0_config);

	readmsr(MSR_IA32_EFER, &low, &high);
	writemsr(MSR_IA32_EFER,low | 0x1, high);

	writemsr(MSR_STAR, 0, SEL_KCSEG | ((SEL_UCSEG - 16) << 16));
	writemsr(MSR_LSTAR, (u32_t)((u64_t)sysenter_entry), (u32_t)((u64_t)sysenter_entry >> 32));
	writemsr(MSR_SFMASK, 512, 0);
	writemsr(MSR_USER_GSBASE, 0, 0);
	writemsr(MSR_KERNEL_GSBASE, (u32_t)((u64_t)(&kernel_stack_info[cpu_id])), (u32_t)((u64_t)(&kernel_stack_info[cpu_id]) >> 32));

#elif defined(__i386__)
	chal_cpu_cr4_set(cr4 | CR4_PSE | CR4_PGE);
	writemsr(IA32_SYSENTER_CS, SEL_KCSEG, 0);
	writemsr(IA32_SYSENTER_ESP, (u32_t)tss[cpu_id].esp0, 0);
	writemsr(IA32_SYSENTER_EIP, (u32_t)sysenter_entry, 0);
#endif

	fpu_init();
	chal_cpu_eflags_init();
}

static inline vaddr_t
chal_cpu_fault_vaddr(struct regs *r)
{
	vaddr_t fault_addr;
#if defined(__x86_64__)
	asm volatile("movq %%cr2, %0" : "=r"(fault_addr));
#elif defined(__i386__)
	asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
#endif

	return fault_addr;
}

/* FIXME: I doubt these flags are really the same as the PGTBL_* macros */
static inline unsigned long
chal_cpu_fault_errcode(struct regs *r)
{
	return r->frame.errcode;
}

static inline unsigned long
chal_cpu_fault_ip(struct regs *r)
{
	return r->frame.ip;
}

static inline void
chal_user_upcall(void *ip, u16_t tid, u16_t cpuid)
{
#if defined(__x86_64__)
	/* rcx = user-level ip, r12 = option, rbx = arg, rax = tid + cpuid  */
	/* $0x3200 : enable interrupt, and iopl is set to 3, the same as user's CPL */
	__asm__("movq $0x3200, %%r11 ; mov %%rdx, %%ds ; movq %3, %%r12 ; sysretq" : : "c"(ip), "a"(tid | (cpuid << 16)), "d"(SEL_UDSEG), "i"(0), "b"(0));
#elif defined(__i386__)
	/* edx = user-level ip, ecx = option, ebx = arg, eax = tid + cpuid */
	__asm__("sti ; sysexit" : : "c"(0), "d"(ip), "b"(0), "a"(tid | (cpuid << 16)));
#endif
}

void chal_timer_thd_init(struct thread *t);

#endif /* CHAL_CPU_H */
