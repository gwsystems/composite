#include "cav7_consts.h"

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
	//      CtrlReg = __cos_cav7_sctlr_get();
	//      CtrlReg &= ~(1<<2);
	//      __cos_cav7_sctlr_set(CtrlReg);
	//      printk("sctlr is %x\n",__cos_cav7_sctlr_get());

	CacheSize = (CsidReg >> 13U) & 0x1FFU;
	CacheSize += 1U;
	CacheSize *= 128U; /* to get number of bytes */

	/* Number of Ways */
	NumWays = (CsidReg & 0x3ffU) >> 3U;
	NumWays += 1U;

	/* Get the cacheline size, way size, index size from csidr */
	LineSize = (CsidReg & 0x07U) + 4U;

	NumSet = CacheSize / 4;
	// NumSet = CacheSize / NumWays;
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

int
chal_l1flush(void)
{
	Xil_L1DCacheFlush();

	return 0;
}

#define ENABLE_PREFETCH

void
l2cache_init(void)
{
	/* 0x410000C8, revision r3p2 */
	printk("L2 cache ID 0x%x\n", CAV7_L2C_CACHE_ID);
	printk("L2 control reg is 0x%x\n", CAV7_L2C_CONTROL);
	printk("L2 aux control reg is 0x%x\n", CAV7_L2C_AUX_CONTROL);
	printk("L2 prefetch control reg is 0x%x\n", CAV7_L2C_PREFETCH_CTRL);
	printk("L2 power control reg is 0x%x\n", CAV7_L2C_POWER_CTRL);

	Xil_L1DCacheFlush();

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

	//      PMCNTENSET[0] = 1;
	//      PMSELR select counter 0.
	//      PMXEVTTYPER event type select Main TLB stall cycles.
	//      PMXEVCNTR clear to zero.
	//          .global             __cos_cav7_pmcr_set
	//          .global             __cos_cav7_pmcntenset_set
	//          .global             __cos_cav7_pmcntenclr_set
	//          .global             __cos_cav7_pmovsr_set
	//          .global             __cos_cav7_pmswinc_set
	//          .global             __cos_cav7_pmselr_set
	//          .global             __cos_cav7_pmccntr_set
	//          .global             __cos_cav7_pmxevtyper_set
	//          .global             __cos_cav7_pmxevcntr_set
	//          .global             __cos_cav7_pmuserenr_set
	//          .global             __cos_cav7_pmintenset_set
	//          .global             __cos_cav7_pmintenclr_set
}
