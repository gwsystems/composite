/* Based on code from Pintos. See LICENSE.pintos for licensing information */

#include "kernel.h"
#include "tss.h"
#include "chal_asm_inc.h"

struct gdt_aligned {
        u64_t seg_descs[SEL_CNT];
} __attribute__((aligned(CACHE_LINE)));
static volatile struct gdt_aligned gdt[NUM_CPU];

/* GDT helpers. */
static u64_t make_code_desc(int dpl);
static u64_t make_data_desc(int dpl);
static u64_t make_data_desc_at(int dpl, u32_t addr);
static u64_t make_tss_desc(void *laddr);
static u64_t make_gdtr_operand(u16_t limit, void *base);

void
chal_tls_update(vaddr_t addr)
{
	int cpu_id = get_cpuid();

	gdt[cpu_id].seg_descs[SEL_UGSEG / sizeof *gdt[cpu_id].seg_descs] = make_data_desc_at(3, (u32_t)addr);
	/* force the reload of the segment cache */
	asm volatile("movl %0, %%gs" : : "q"(SEL_UGSEG));
}

/*
 * Sets up a proper GDT.  The bootstrap loader's GDT didn't include
 * user-mode selectors or a TSS, but we need both now.
 */
void
gdt_init(const cpuid_t cpu_id)
{
	u64_t gdtr_operand;

	/* Initialize GDT. */
	gdt[cpu_id].seg_descs[SEL_NULL / sizeof *gdt[cpu_id].seg_descs]  = 0;
	gdt[cpu_id].seg_descs[SEL_KCSEG / sizeof *gdt[cpu_id].seg_descs] = make_code_desc(0);
	gdt[cpu_id].seg_descs[SEL_KDSEG / sizeof *gdt[cpu_id].seg_descs] = make_data_desc(0);
	gdt[cpu_id].seg_descs[SEL_UCSEG / sizeof *gdt[cpu_id].seg_descs] = make_code_desc(3);
	gdt[cpu_id].seg_descs[SEL_UDSEG / sizeof *gdt[cpu_id].seg_descs] = make_data_desc(3);
	gdt[cpu_id].seg_descs[SEL_TSS / sizeof *gdt[cpu_id].seg_descs]   = make_tss_desc(&(tss[cpu_id]));
	gdt[cpu_id].seg_descs[SEL_UGSEG / sizeof *gdt[cpu_id].seg_descs] = make_data_desc(3);

	/*
	 * Load GDTR, TR.  See [IA32-v3a] 2.4.1 "Global Descriptor
	 * Table Register (GDTR)", 2.4.4 "Task Register (TR)", and
	 * 6.2.4 "Task Register".
	 */
	gdtr_operand = make_gdtr_operand(sizeof gdt[cpu_id] - 1, (void *)(gdt + cpu_id));
	asm volatile("lgdt %0" : : "m"(gdtr_operand));
	asm volatile("ltr %w0" : : "q"(SEL_TSS));
}

/* System segment or code/data segment? */
enum seg_class
{
	CLS_SYSTEM    = 0, /* System segment. */
	CLS_CODE_DATA = 1  /* Code or data segment. */
};

/* Limit has byte or 4 kB page granularity? */
enum seg_granularity
{
	GRAN_BYTE = 0, /* Limit has 1-byte granularity. */
	GRAN_PAGE = 1  /* Limit has 4 kB granularity. */
};

/* Returns a segment descriptor with the given 32-bit BASE and
   20-bit LIMIT (whose interpretation depends on GRANULARITY).
   The descriptor represents a system or code/data segment
   according to CLASS, and TYPE is its type (whose interpretation
   depends on the class).

   The segment has descriptor privilege level DPL, meaning that
   it can be used in rings numbered DPL or lower.  In practice,
   DPL==3 means that user processes can use the segment and
   DPL==0 means that only the kernel can use the segment.  See
   [IA32-v3a] 4.5 "Privilege Levels" for further discussion. */
static u64_t
make_seg_desc(u32_t base, u32_t limit, enum seg_class class, int type, int dpl, enum seg_granularity granularity)
{
	u32_t e0, e1;

	// ASSERT (limit <= 0xfffff);
	// ASSERT (class == CLS_SYSTEM || class == CLS_CODE_DATA);
	// ASSERT (type >= 0 && type <= 15);
	// ASSERT (dpl >= 0 && dpl <= 3);
	// ASSERT (granularity == GRAN_BYTE || granularity == GRAN_PAGE);

	e0 = ((limit & 0xffff) /* Limit 15:0. */
	      | (base << 16)); /* Base 15:0. */

	e1 = (((base >> 16) & 0xff)   /* Base 23:16. */
	      | (type << 8)           /* Segment type. */
	      | (class << 12)         /* 0=system, 1=code/data. */
	      | (dpl << 13)           /* Descriptor privilege. */
	      | (1 << 15)             /* Present. */
	      | (limit & 0xf0000)     /* Limit 16:19. */
	      | (1 << 22)             /* 32-bit segment. */
	      | (granularity << 23)   /* Byte/page granularity. */
	      | (base & 0xff000000)); /* Base 31:24. */

	return e0 | ((u64_t)e1 << 32);
}

/*
 * Returns a descriptor for a readable code segment with base at 0, a
 *  limit of 4 GB, and the given DPL.
 */
static u64_t
make_code_desc(int dpl)
{
	return make_seg_desc(0, 0xfffff, CLS_CODE_DATA, 10, dpl, GRAN_PAGE);
}

/*
 * Returns a descriptor for a writable data segment with base at 0, a
 * limit of 4 GB, and the given DPL.
 */
static u64_t
make_data_desc(int dpl)
{
	return make_seg_desc(0, 0xfffff, CLS_CODE_DATA, 2, dpl, GRAN_PAGE);
}

static u64_t
make_data_desc_at(int dpl, u32_t base)
{
	return make_seg_desc(base, 0xfffff, CLS_CODE_DATA, 2, dpl, GRAN_PAGE);
}

/*
 * Returns a descriptor for an "available" 32-bit Task-State Segment
 *  with its base at the given linear address, a limit of 0x67 bytes
 *  (the size of a 32-bit TSS), and a DPL of 0.  See [IA32-v3a] 6.2.2
 *  "TSS Descriptor".
 */
static u64_t
make_tss_desc(void *laddr)
{
	return make_seg_desc((u32_t)laddr, 0x67, CLS_SYSTEM, 9, 0, GRAN_BYTE);
}

/*
 * Returns a descriptor that yields the given LIMIT and BASE when
 * used as an operand for the LGDT instruction.
 */
static u64_t
make_gdtr_operand(u16_t limit, void *base)
{
	return limit | ((u64_t)(u32_t)base << 16);
}
