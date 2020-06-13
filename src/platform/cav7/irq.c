#include <chal.h>
#include <shared/cos_types.h>
#include "kernel.h"
#include "mem_layout.h"
#include "chal_cpu.h"
#include "irq.h"

/* Base of registers */
/* What is the interrupt priority grouping? */
#define CAV7_GIC_GROUPING CAV7_GIC_GROUPING_P7S1
/* GIC distributor base address */
#define CAV7_GICD_BASE 0xF8F01000
/* GIC CPU interface base address */
#define CAV7_GICC_BASE 0xF8F00100
/* Private timer and watchdog block base */
#define CAV7_PTWD_BASE 0xF8F00600


/* Macro definitions for timers and interrupt controllers */
#define CAV7_SFR(base, offset) (*((volatile unsigned long *)((unsigned long)((base) + (offset)))))
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

void
__cos_cav7_int_local_init(void)
{
	/* Priority grouping */
	CAV7_GICC_BPR = CAV7_GIC_GROUPING;

	/* Enable all interrupts to this interface - FIQ is bypassed, and all
	 * interrupts go through the IRQ. The FIQ feature is only available on
	 * the standalone FIQ interrupt line */
	CAV7_GICC_CTLR = CAV7_GICC_ENABLEGRP0;

	/* No interrupts are masked - This must be set at last because enabling
	 * will trash the contents of this register if previously set. To maintain
	 * compatibility across all possible implementations, no priority level
	 * lower than 0xF0 will be considered valid */
	CAV7_GICC_PMR = 0xF0U;
}

void
__cos_cav7_timer_init(void)
{
	/* Do nothing - we will initialize the timer in another function */
}

void
__cos_cav7_int_init(void)
{
	unsigned long temp;
	unsigned long lines;

	/* Who implemented the GIC, what variant, how many interrupts? */
	temp = CAV7_GICD_IIDR;
	printk("CAV7-GIC - ProductID: %d, Variant: %d, Revision: %d, Implementer: 0x%x\r\n", temp >> 24,
	       (temp >> 20) & 0xF, (temp >> 12) & 0xF, temp & 0xFFF);

	/* How many locked SPIs, security extension enabled or not, number of
	 * actual CPUs and interrupt lines */
	temp  = CAV7_GICD_TYPER;
	lines = ((temp & 0x1F) + 1) * 32;
	printk("CAV7-GIC - SPI number: %d, Security extension: %d, CPU number: %d, Interrupt line number: %d\r\n",
	       temp >> 16, (temp >> 10) & 0x1, ((temp >> 5) & 0x7) + 1, lines);

	/* Initialize all vectors to group 0, and disable all */
	for (temp = 0; temp < lines / 32; temp++) {
		CAV7_GICD_ICPENDR(temp)   = 0xFFFFFFFFU;
		CAV7_GICD_IGROUPR(temp)   = 0x00000000U;
		CAV7_GICD_ICENABLER(temp) = 0xFFFFFFFFU;
	}

	/* Set the priority of all such interrupts to the lowest level */
	for (temp = 0; temp < lines / 4; temp++) CAV7_GICD_IPRIORITYR(temp) = 0xA0A0A0A0U;

	/* All interrupts target CPU0 */
	for (temp = 8; temp < lines / 4; temp++) CAV7_GICD_ITARGETSR(temp) = 0x01010101U;

	/* All interrupts are edge triggered, and use 1-N model */
	for (temp = 0; temp < lines / 16; temp++) CAV7_GICD_ICFGR(temp) = 0x55555555U;

	/* Enable the interrupt controller */
	CAV7_GICD_CTLR = CAV7_GICD_CTLR_GRP1EN | CAV7_GICD_CTLR_GRP0EN;

	/* Initialize the core-local interrupt controller */
	__cos_cav7_int_local_init();
}

/* Private timer and watchdog block base */
#define CAV7_PTWD_BASE 0xF8F00600

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

void
__cos_cav7_irq_handler(struct pt_regs regs)
{
	unsigned long int_id;
	unsigned long cpuid;

	/* What interrupt is this? */
	int_id = CAV7_GICC_IAR;
	cpuid  = int_id >> 10;
	int_id &= 0x3FFU;

	/* Is this a spurious interrupt? (Can't be 1022 because GICv1 don't have group1) */
	assert(int_id != 1022);
	/* Spurious interrupt does not need this at all */
	if (int_id == 1023) return;
	/* Only the booting processor will receive timer interrupts */
	/* Is is an timer interrupt? (we know that it is at 29) */
	if (int_id == 29) {
		/* Clear the interrupt flag */
		CAV7_PTWD_PTISR = 0;
		/* printk("tptsc\n"); */
		timer_process(regs);
		/* Send interrupt to all other processors to notify them about this */
		/* EOI the interrupt */
		CAV7_GICC_EOIR = int_id;
		return;
	}

	/* Is this an casual interrupt? */
	assert(cpuid == 0);
	/* Not handled. lol */
}
