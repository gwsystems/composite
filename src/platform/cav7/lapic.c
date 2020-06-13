#include "kernel.h"
#include "chal_cpu.h"
#include "shared/cpu_ghz.h"

/* Base of registers */
/* What is the interrupt priority grouping? */
#define CAV7_GIC_GROUPING CAV7_GIC_GROUPING_P7S1
/* GIC distributor base address */
#define CAV7_GICD_BASE 0xF8F01000
/* GIC CPU interface base address */
#define CAV7_GICC_BASE 0xF8F00100
/* Private timer and watchdog block base */
#define CAV7_PTWD_BASE 0xF8F00600
/* Global timer base */
#define CAV7_GTMR_BASE 0xF8F00200

/* Macro definitions for timers and interrupt controllers */
#define CAV7_SFR(base, offset) (*((volatile unsigned long *)((unsigned long)((base) + (offset)))))

/* Timer definitions */
/* Private timer load register */
#define CAV7_PTWD_PTLR CAV7_SFR(CAV7_PTWD_BASE, 0x0000)
/* Private timer counter register */
#define CAV7_PTWD_PTCNTR CAV7_SFR(CAV7_PTWD_BASE, 0x0004)
/* Private timer control register */
#define CAV7_PTWD_PTCTLR CAV7_SFR(CAV7_PTWD_BASE, 0x0008)
#define CAV7_PTWD_PTCTLR_PRESC(x) ((x) << 8U)
#define CAV7_PTWD_PTCTLR_IRQEN (1U << 2U)
#define CAV7_PTWD_PTCTLR_AUTOREL (1U << 1U)
#define CAV7_PTWD_PTCTLR_TIMEN (1U << 0U)
/* Private timer interrupt status register */
#define CAV7_PTWD_PTISR CAV7_SFR(CAV7_PTWD_BASE, 0x000C)

/* Watchdog load register */
#define CAV7_PTWD_WDLR CAV7_SFR(CAV7_PTWD_BASE, 0x0020)
/* Watchdog counter register */
#define CAV7_PTWD_WDCNTR CAV7_SFR(CAV7_PTWD_BASE, 0x0024)
/* Watchdog control register */
#define CAV7_PTWD_WDCTLR CAV7_SFR(CAV7_PTWD_BASE, 0x0028)
/* Watchdog interrupt status register */
#define CAV7_PTWD_WDISR CAV7_SFR(CAV7_PTWD_BASE, 0x002C)
/* Watchdog reset status register */
#define CAV7_PTWD_WDRSR CAV7_SFR(CAV7_PTWD_BASE, 0x0030)
/* Watchdog disable register */
#define CAV7_PTWD_WDDR CAV7_SFR(CAV7_PTWD_BASE, 0x0034)

/* GIC definitions - This is only present in MPCore platform. Most Cortex-A9s,
 * all Cortex-A7s and Cortex-A5s are on the multicore platform */
/* Distributor control register */
#define CAV7_GICD_CTLR CAV7_SFR(CAV7_GICD_BASE, 0x0000)
#define CAV7_GICD_CTLR_GRP1EN (1U << 1U)
#define CAV7_GICD_CTLR_GRP0EN (1U << 0U)
/* Interrupt controller type register */
#define CAV7_GICD_TYPER CAV7_SFR(CAV7_GICD_BASE, 0x0004)
/* Distributor implementer identification register */
#define CAV7_GICD_IIDR CAV7_SFR(CAV7_GICD_BASE, 0x0008)
/* Interrupt group registers */
#define CAV7_GICD_IGROUPR(x) CAV7_SFR(CAV7_GICD_BASE, 0x0080 + (x)*4) /* 0-31 */
/* Interrupt enabling registers */
#define CAV7_GICD_ISENABLER(x) CAV7_SFR(CAV7_GICD_BASE, 0x0100 + (x)*4) /* 0-31 */
/* Interrupt disabling registers */
#define CAV7_GICD_ICENABLER(x) CAV7_SFR(CAV7_GICD_BASE, 0x0180 + (x)*4) /* 0-31 */
/* Interrupt set-pending registers */
#define CAV7_GICD_ISPENDR(x) CAV7_SFR(CAV7_GICD_BASE, 0x0200 + (x)*4) /* 0-31 */
/* Interrupt clear-pending registers */
#define CAV7_GICD_ICPENDR(x) CAV7_SFR(CAV7_GICD_BASE, 0x0280 + (x)*4) /* 0-31 */
/* Interrupt set-active registers */
#define CAV7_GICD_ISACTIVER(x) CAV7_SFR(CAV7_GICD_BASE, 0x0300 + (x)*4) /* 0-31 */
/* Interrupt clear-active registers */
#define CAV7_GICD_ICACTIVER(x) CAV7_SFR(CAV7_GICD_BASE, 0x0380 + (x)*4) /* 0-31 */
/* Interrupt priority registers */
#define CAV7_GICD_IPRIORITYR(x) CAV7_SFR(CAV7_GICD_BASE, 0x0400 + (x)*4) /* 0-254 */
/* Interrupt processor targets registers */
#define CAV7_GICD_ITARGETSR(x) CAV7_SFR(CAV7_GICD_BASE, 0x0800 + (x)*4) /* 0-7 */
/* Interrupt configuration registers */
#define CAV7_GICD_ICFGR(x) CAV7_SFR(CAV7_GICD_BASE, 0x0C00 + (x)*4) /* 0-63 */
/* Non-secure access control registers */
#define CAV7_GICD_NSACR(x) CAV7_SFR(CAV7_GICD_BASE, 0x0E00 + (x)*4) /* 0-63 */
/* Software generated interrupt register */
#define CAV7_GICD_SGIR CAV7_SFR(CAV7_GICD_BASE, 0x0F00)
/* SGI clear-pending registers */
#define CAV7_GICD_CPENDSGIR(x) CAV7_SFR(CAV7_GICD_BASE, 0x0F10 + (x)*4) /* 0-3 */
/* SGI set-pending registers */
#define CAV7_GICD_SPENDSGIR(x) CAV7_SFR(CAV7_GICD_BASE, 0x0F20 + (x)*4) /* 0-3 */

/* CPU interface control register */
#define CAV7_GICC_CTLR CAV7_SFR(CAV7_GICC_BASE, 0x0000)
/* Interrupt priority mask register */
#define CAV7_GICC_PMR CAV7_SFR(CAV7_GICC_BASE, 0x0004)
/* Binary pointer register */
#define CAV7_GICC_BPR CAV7_SFR(CAV7_GICC_BASE, 0x0004)
/* Interrupt acknowledge register */
#define CAV7_GICC_IAR CAV7_SFR(CAV7_GICC_BASE, 0x000C)
/* End of interrupt register */
#define CAV7_GICC_EOIR CAV7_SFR(CAV7_GICC_BASE, 0x0010)
/* Running priority register */
#define CAV7_GICC_RPR CAV7_SFR(CAV7_GICC_BASE, 0x0014)
/* Highest priority pending interrupt register */
#define CAV7_GICC_HPPIR CAV7_SFR(CAV7_GICC_BASE, 0x0018)
/* Aliased binary point register */
#define CAV7_GICC_ABPR CAV7_SFR(CAV7_GICC_BASE, 0x001C)
/* Aliased interrupt acknowledge register */
#define CAV7_GICC_AIAR CAV7_SFR(CAV7_GICC_BASE, 0x0020)
/* Aliased end of interrupt register */
#define CAV7_GICC_AEOIR CAV7_SFR(CAV7_GICC_BASE, 0x0024)
/* Aliased highest priority pending interrupt register */
#define CAV7_GICC_AHPPIR CAV7_SFR(CAV7_GICC_BASE, 0x0028)
/* Active priorities registers */
#define CAV7_GICC_APR(x) CAV7_SFR(CAV7_GICC_BASE, 0x00D0 + (x)*4) /* 0-3 */
/* Non-secure active priorities registers */
#define CAV7_GICC_NSAPR(x) CAV7_SFR(CAV7_GICC_BASE, 0x00E0 + (x)*4) /* 0-3 */
/* CPU interface identification register */
#define CAV7_GICC_IIDR CAV7_SFR(CAV7_GICC_BASE, 0x00FC)
/* Deactivate interrupt register */
#define CAV7_GICC_DIR CAV7_SFR(CAV7_GICC_BASE, 0x1000)

/* Control register */
#define CAV7_GICC_CBPR (1U << 4U)
#define CAV7_GICC_FIQEN (1U << 3U)
#define CAV7_GICC_ACKCTL (1U << 2U)
#define CAV7_GICC_ENABLEGRP1 (1U << 1U)
#define CAV7_GICC_ENABLEGRP0 (1U << 0U)

/* Priority grouping */
#define CAV7_GIC_GROUPING_P7S1 (0U)
#define CAV7_GIC_GROUPING_P6S2 (1U)
#define CAV7_GIC_GROUPING_P5S3 (2U)
#define CAV7_GIC_GROUPING_P4S4 (3U)
#define CAV7_GIC_GROUPING_P3S5 (4U)
#define CAV7_GIC_GROUPING_P2S6 (5U)
#define CAV7_GIC_GROUPING_P1S7 (6U)
#define CAV7_GIC_GROUPING_P0S8 (7U)

/* Global timer registers */
#define CAV7_GTMR_GTCNTRL CAV7_SFR(CAV7_GTMR_BASE, 0x0000)
#define CAV7_GTMR_GTCNTRH CAV7_SFR(CAV7_GTMR_BASE, 0x0004)
#define CAV7_GTMR_GTCTLR CAV7_SFR(CAV7_GTMR_BASE, 0x0008)

void
chal_timer_set(cycles_t cycles)
{
	chal_timer_disable();
	// cycles = 7670*100;
	// printk("cycles %lld\n", cycles);
	/* Writing this will also write the counter register as well */
	CAV7_PTWD_PTLR = cycles / 2;
	/* CAV7_PTWD_PTCNTR = cycles/2; */
	/* Clear the interrupt flag - write 1! */
	CAV7_PTWD_PTISR = 1;
	/* Ack all timer interrupts */
	CAV7_GICC_EOIR = 29;
	/* Enable the interrupt */
	CAV7_GICD_ISENABLER(0) = 1 << 29;
	/* Start the timer */
	CAV7_PTWD_PTCTLR = CAV7_PTWD_PTCTLR_PRESC(0) | CAV7_PTWD_PTCTLR_IRQEN | CAV7_PTWD_PTCTLR_TIMEN;
}

void
chal_timer_disable(void)
{
	/* Disable the interrupt */
	CAV7_GICD_ICENABLER(0) = 0 << 29;
	CAV7_PTWD_PTCTLR       = 0;
	/* Clear the interrupt flag - write 1! */
	CAV7_PTWD_PTISR = 1;
}

unsigned int
chal_cyc_thresh(void)
{
	return 76700;
}

void
lapic_timer_init(void)
{
	/* We are initializing the global timer here */
	CAV7_GTMR_GTCNTRL = 0;
	CAV7_GTMR_GTCNTRH = 0;
	CAV7_GTMR_GTCTLR  = 1;
	printk("global timer init\n");
}

static int
lapic_ipi_send(u32_t dest, u32_t vect_flags)
{
	return 0;
}

/* HACK: assume that the HZ of the processor is equivalent to that on the computer used for compilation. */
static void
delay_us(u32_t us)
{
	unsigned long long          hz = CPU_GHZ, hz_per_us = hz * 1000;
	unsigned long long          end;
	volatile unsigned long long tsc;

	rdtscll(tsc);
	end = tsc + (hz_per_us * us);
	while (1) {
		rdtscll(tsc);
		if (tsc >= end) return;
	}
}

/* The SMP boot patchcode from loader.S */
extern char smppatchstart, smppatchend, smpstack, stack;

void
smp_boot_all_ap(volatile int *cores_ready)
{
}

void
smp_init(volatile int *cores_ready)
{
	smp_boot_all_ap(cores_ready);
}
#define CAV7_L2C_BASE 0xF8F02000
/* Cache ID register */
#define CAV7_L2C_CACHE_ID CAV7_SFR(CAV7_L2C_BASE, 0x0000)
/* Cache type register */
#define CAV7_L2C_CACHE_TYPE CAV7_SFR(CAV7_L2C_BASE, 0x0004)
/* Control register */
#define CAV7_L2C_CONTROL CAV7_SFR(CAV7_L2C_BASE, 0x0100)
/* Auxiliary control register */
#define CAV7_L2C_AUX_CONTROL CAV7_SFR(CAV7_L2C_BASE, 0x0104)
/* Tag and data RAM latency control registers */
#define CAV7_L2C_TAG_RAM_CTRL CAV7_SFR(CAV7_L2C_BASE, 0x0108)
#define CAV7_L2C_DATA_RAM_CTRL CAV7_SFR(CAV7_L2C_BASE, 0x010C)
/* Event counter control register */
#define CAV7_L2C_EV_CNTR_CTRL CAV7_SFR(CAV7_L2C_BASE, 0x0200)
/* Event counter configuration registers */
#define CAV7_L2C_EV_CNTR1_CFG CAV7_SFR(CAV7_L2C_BASE, 0x0204)
#define CAV7_L2C_EV_CNTR0_CFG CAV7_SFR(CAV7_L2C_BASE, 0x0208)
/* Event counter value registers */
#define CAV7_L2C_EV_CNTR1 CAV7_SFR(CAV7_L2C_BASE, 0x020C)
#define CAV7_L2C_EV_CNTR0 CAV7_SFR(CAV7_L2C_BASE, 0x0210)
/* Interrupt registers */
#define CAV7_L2C_INT_MASK CAV7_SFR(CAV7_L2C_BASE, 0x0214)
#define CAV7_L2C_INT_MASK_STATUS CAV7_SFR(CAV7_L2C_BASE, 0x0218)
#define CAV7_L2C_INT_RAW_STATUS CAV7_SFR(CAV7_L2C_BASE, 0x021C)
#define CAV7_L2C_INT_CLEAR CAV7_SFR(CAV7_L2C_BASE, 0x0220)
/* Cache maintenance operations */
#define CAV7_L2C_CACHE_SYNC CAV7_SFR(CAV7_L2C_BASE, 0x0730)
#define CAV7_L2C_INV_PA CAV7_SFR(CAV7_L2C_BASE, 0x0770)
#define CAV7_L2C_INV_WAY CAV7_SFR(CAV7_L2C_BASE, 0x077C)
#define CAV7_L2C_CLEAN_PA CAV7_SFR(CAV7_L2C_BASE, 0x07B0)
#define CAV7_L2C_CLEAN_INDEX CAV7_SFR(CAV7_L2C_BASE, 0x07B8)
#define CAV7_L2C_CLEAN_WAY CAV7_SFR(CAV7_L2C_BASE, 0x07BC)
#define CAV7_L2C_CLEAN_INV_PA CAV7_SFR(CAV7_L2C_BASE, 0x07F0)
#define CAV7_L2C_CLEAN_INV_INDEX CAV7_SFR(CAV7_L2C_BASE, 0x07F8)
#define CAV7_L2C_CLEAN_INV_WAY CAV7_SFR(CAV7_L2C_BASE, 0x07FC)
/* Cache lockdown */
#define CAV7_L2C_D_LOCKDOWN(X) CAV7_SFR(CAV7_L2C_BASE, 0x0900 + (X)*8) /* 0-7 */
#define CAV7_L2C_I_LOCKDOWN(X) CAV7_SFR(CAV7_L2C_BASE, 0x0904 + (X)*8) /* 0-7 */
#define CAV7_L2C_D_LOCK_LINE_EN CAV7_SFR(CAV7_L2C_BASE, 0x0950)
#define CAV7_L2C_I_UNLOCK_WAY CAV7_SFR(CAV7_L2C_BASE, 0x0954)
/* Address filtering */
#define CAV7_L2C_I_ADDR_FLTR_START CAV7_SFR(CAV7_L2C_BASE, 0x0C00)
#define CAV7_L2C_I_ADDR_FLTR_END CAV7_SFR(CAV7_L2C_BASE, 0x0C04)
/* Debug register */
#define CAV7_L2C_DEBUG_CTRL CAV7_SFR(CAV7_L2C_BASE, 0x0F40)
/* Prefetch control register */
#define CAV7_L2C_PREFETCH_CTRL CAV7_SFR(CAV7_L2C_BASE, 0x0F60)
/* Power control register */
#define CAV7_L2C_POWER_CTRL CAV7_SFR(CAV7_L2C_BASE, 0x0F80)

void
Xil_L1DCacheFlush(void)
{
	register unsigned long CsidReg, C7Reg, CtrlReg;
	unsigned long          CacheSize, LineSize, NumWays;
	unsigned long          Way;
	unsigned long          WayIndex, Set, SetIndex, NumSet;
	unsigned long          currmask;

	/* Select cache level 0 and D cache in CSSR */
	__cos_cav7_id_csselr_set(0);

	CsidReg = __cos_cav7_id_ccsidr_get();

	/* Determine Cache Size */

	/* disable the Data cache */
	//	CtrlReg = __cos_cav7_sctlr_get();
	//	CtrlReg &= ~(1<<2);
	//	__cos_cav7_sctlr_set(CtrlReg);
	//	printk("sctlr is %x\n",__cos_cav7_sctlr_get());

	CacheSize = (CsidReg >> 13U) & 0x1FFU;
	CacheSize += 1U;
	CacheSize *= 128U; /* to get number of bytes */

	/* Number of Ways */
	NumWays = (CsidReg & 0x3ffU) >> 3U;
	NumWays += 1U;

	/* Get the cacheline size, way size, index size from csidr */
	LineSize = (CsidReg & 0x07U) + 4U;

	NumSet = CacheSize / 4;
	//NumSet = CacheSize / NumWays;
	NumSet /= (0x00000001U << LineSize);

	Way = 0U;
	Set = 0U;

	/* Invalidate all the cachelines */
	for (WayIndex = 0U; WayIndex < NumWays; WayIndex++) {
		for (SetIndex = 0U; SetIndex < NumSet; SetIndex++) {
			C7Reg = Way | Set;
			/* Flush by Set/Way */
			__cos_cav7_dccisw_set(C7Reg);
			Set += (0x00000001U << LineSize);
		}
		Set = 0U;
		Way += 0x40000000U;
	}

	/* disable the Data cache
	CtrlReg = __cos_cav7_sctlr_get();
	CtrlReg &= ~(1<<2);
	__cos_cav7_sctlr_set(CtrlReg); */
}

//#define ENABLE_PREFETCH

void
l2cache_init(void)
{
	/* 0x410000C8, revision r3p2 */
	printk("L2 cache ID 0x%x\n", CAV7_L2C_CACHE_ID);
	printk("L2 control reg is 0x%x\n", CAV7_L2C_CONTROL);
	printk("L2 aux control reg is 0x%x\n", CAV7_L2C_AUX_CONTROL);
	printk("L2 prefetch control reg is 0x%x\n", CAV7_L2C_PREFETCH_CTRL);
	printk("L2 power control reg is 0x%x\n", CAV7_L2C_POWER_CTRL);

	/* Just enable the prefetcher in general settings, but keep them disabled at the specific controls, only
	 * instruction pf enabled */
#ifndef ENABLE_PREFETCH
	CAV7_L2C_AUX_CONTROL &= 0x2fbfffff;
	CAV7_L2C_AUX_CONTROL |= 0x00400000;
#endif
	/* Invalidate all of them and wait */
	CAV7_L2C_INV_WAY = 0xFFFF;
	while ((CAV7_L2C_INV_WAY & 0xFFFF) != 0)
		;
	/* Clear all interrupts */
	CAV7_L2C_INT_CLEAR = 0xFFFFFFFF;
	/* Enable L2 cache */
	CAV7_L2C_CONTROL = 0x01;

	printk("L2 cache ID 0x%x\n", CAV7_L2C_CACHE_ID);
	printk("L2 control reg is 0x%x\n", CAV7_L2C_CONTROL);
	printk("L2 aux control reg is 0x%x\n", CAV7_L2C_AUX_CONTROL);
	printk("L2 prefetch control reg is 0x%x\n", CAV7_L2C_PREFETCH_CTRL);
	printk("L2 power control reg is 0x%x\n", CAV7_L2C_POWER_CTRL);

	/* Program the ACTLR to turn off the L1 prefetching as well */
#ifndef ENABLE_PREFETCH
	printk("ACTLR reg is 0x%x\n", __cos_cav7_actlr_get());
	unsigned long actlr = __cos_cav7_actlr_get();
	__cos_cav7_actlr_set(actlr & 0xFFFFFFF9);
#endif
	printk("ACTLR reg is 0x%x\n", __cos_cav7_actlr_get());

	/* We initialize the PMU to count main TLB stall */
	unsigned long pmcr = __cos_cav7_pmcr_get();
	printk("PMCR reg is 0x%x\n", pmcr); /* IMP = 0x41 IDCODE = 0x09 N = 00110b = 0x6, divider on, all counters
	                                       enabled - leave unchanged */
	/* Enable the first counter */
	unsigned long pmcntenset = __cos_cav7_pmcntenset_get();
	printk("PMCNTENSET reg is 0x%x\n", pmcntenset);
	__cos_cav7_pmcntenset_set(pmcntenset | 0x01);
	printk("PMCNTENSET reg is 0x%x\n", __cos_cav7_pmcntenset_get());
	/* Select the first counter */
	__cos_cav7_pmselr_set(0x00);
	printk("PMSELR reg is 0x%x\n", __cos_cav7_pmselr_get());
	/* Set the event type to main TLB stall 0x62
	 * - or 0x83 data main TLB miss stall
	 * - or 0x85 data micro TLB stall, we can measure main TLB lockdown */
	__cos_cav7_pmxevtyper_set(0x83);
	printk("PMXEVRTPER reg is 0x%x\n", __cos_cav7_pmxevtyper_get());
	/* Clear the counter value to zero */
	__cos_cav7_pmxevcntr_set(0x00);
	printk("PMXEVRTPER reg is 0x%x\n", __cos_cav7_pmxevcntr_get());
	/* Debug features? */
	printk("ID_DFR0 reg is 0x%x\n", __cos_cav7_id_dfr0_get());

	//	PMCNTENSET[0] = 1;
	//	PMSELR select counter 0.
	//	PMXEVTTYPER event type select Main TLB stall cycles.
	//	PMXEVCNTR clear to zero.
	//	    .global             __cos_cav7_pmcr_set
	//	    .global             __cos_cav7_pmcntenset_set
	//	    .global             __cos_cav7_pmcntenclr_set
	//	    .global             __cos_cav7_pmovsr_set
	//	    .global             __cos_cav7_pmswinc_set
	//	    .global             __cos_cav7_pmselr_set
	//	    .global             __cos_cav7_pmccntr_set
	//	    .global             __cos_cav7_pmxevtyper_set
	//	    .global             __cos_cav7_pmxevcntr_set
	//	    .global             __cos_cav7_pmuserenr_set
	//	    .global             __cos_cav7_pmintenset_set
	//	    .global             __cos_cav7_pmintenclr_set
}

int
chal_tlb_lockdown(unsigned long entryid, unsigned long vaddr, unsigned long paddr)
{
	/* entryid: 0-3 means lock a 4kB page,
	 * 4-7 means lock a 64kB page,
	 * 8-11 means lock a 1MB page,
	 * 12-15 means lock a 16MB page */
	assert(entryid < 16);
	__cos_cav7_sel_lkdn_write_set(entryid % 4);
	if (entryid < 4) {
		/* Cortex-A9 main TLB VA register */
		__cos_cav7_main_tlb_va_set(vaddr);
		/* Cortex-A9 main TLB attribute register */
		__cos_cav7_main_tlb_attrib_set(0x07);
		/* Cortex-A9 main TLB PA register */
		__cos_cav7_main_tlb_pa_set(paddr | (1 << 6) | (3 << 1) | 1);
	} else if (entryid < 8) {
		__cos_cav7_main_tlb_va_set(vaddr);
		__cos_cav7_main_tlb_attrib_set(0x07);
		__cos_cav7_main_tlb_pa_set(paddr | (2 << 6) | (3 << 1) | 1);
	} else if (entryid < 12) {
		__cos_cav7_main_tlb_va_set(vaddr);
		__cos_cav7_main_tlb_attrib_set(0x07);
		__cos_cav7_main_tlb_pa_set(paddr | (3 << 6) | (3 << 1) | 1);
	} else {
		__cos_cav7_main_tlb_va_set(vaddr);
		__cos_cav7_main_tlb_attrib_set(0x07);
		__cos_cav7_main_tlb_pa_set(paddr | (0 << 6) | (3 << 1) | 1);
	}

	__cos_cav7_ats1cpr_set(vaddr);
	assert((__cos_cav7_par_get() & 0xFFFFF000) == paddr);

	return 0;
}
