#include <chal.h>
#include <types.h>
#include <kernel.h>
#include <chal_cpu.h>
#include <consts.h>
#include <compiler.h>

char timer_detector[COS_PAGE_SIZE] COS_PAGE_ALIGNED;

paddr_t chal_kernel_mem_pa;
u32_t chal_msr_mhz;

struct cpu_tlb_asid_map tlb_asid_map[COS_NUM_CPU];

void
chal_send_ipi(coreid_t cpu_id)
{
	lapic_asnd_ipi_send(cpu_id);

	return;
}

__attribute__((noreturn)) void
chal_khalt(void)
{
	khalt();
}

int
chal_tlb_lockdown(unsigned long entryid, unsigned long vaddr, unsigned long paddr)
{
	return 0;
}

int
chal_tlbflush(int a)
{
	/* TODO */
	return 0;
}

void
chal_init(void)
{
	u32_t a = 0, b = 0, c = 0, d = 0;
	u32_t vendor[4];

	printk("Processor information:\n");
	a = 0;
	chal_cpuid(&a, &b, &c, &d);
	vendor[0] = b;
	vendor[1] = d;
	vendor[2] = c;
	vendor[3] = 0;
	printk("\tVendor: %s\n", (char *)vendor);

	a = 0x80000000;
	chal_cpuid(&a, &b, &c, &d);
	/* processor brand string is supported? */
	if (a > 0x80000004) {
		u32_t name[13];

		name[0] = 0x80000002;
		chal_cpuid(&name[0], &name[1], &name[2], &name[3]);
		name[4] = 0x80000003;
		chal_cpuid(&name[4], &name[5], &name[6], &name[7]);
		name[8] = 0x80000004;
		chal_cpuid(&name[8], &name[9], &name[10], &name[11]);
		name[12] = 0;

		printk("\tBrand string %s\n", (char *)name);
	}

	printk("\tFeatures [");
	a = 1;
	chal_cpuid(&a, &b, &c, &d);
	if (d & (1 << 4)) printk("rdtsc ");
	if (d & (1 << 0)) printk("fpu ");
	if (d & (1 << 3)) printk("superpages ");
	if (d & (1 << 11)) printk("sysenter/exit ");
	if (d & (1 << 19)) printk("clflush ");
	if (d & (1 << 22)) printk("acpi ");
	if (d & (1 << 9)) printk("apic ");
	if (c & (1 << 21)) printk("x2apic ");
	if (c & (1 << 24)) printk("tsc-deadline ");
	a = 0x80000001;
	chal_cpuid(&a, &b, &c, &d);
	if (d & (1 << 27)) printk("rdtscp ");
	a = 0x80000007;
	chal_cpuid(&a, &b, &c, &d);
	if (d & (1 << 8)) printk("invariant_tsc ");
	printk("]\n");

	a = 0x16;
	chal_cpuid(&a, &b, &c, &d);
	/* FIXME: on x86_64, need to do cpuid twice to get frequency, don't know why */
	a = 0x16;
	chal_cpuid(&a, &b, &c, &d);
	a = (a << 16) >> 16;
	if (a) {
		printk("\tCPUID base frequency: %d (* 1Mhz)\n", a);
		printk("\tCPUID max  frequency: %d (* 1Mhz)\n", (b << 16) >> 16);
	}

	/* FIXME: on x86_64, cannot get platform info on qemu */
	readmsr(MSR_PLATFORM_INFO, &a, &b);
	a = (a >> 8) & ((1<<7)-1);
	if (a) {
		printk("\tMSR Frequency: %d (* 100Mhz)\n", a);
		chal_msr_mhz = a * 100;
	}
}
