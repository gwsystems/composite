#ifndef CAV7_CONSTS_H
#define CAV7_CONSTS_H

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

#endif /* CAV7_CONSTS_H */
