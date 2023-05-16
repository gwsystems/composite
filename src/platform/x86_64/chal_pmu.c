#ifndef CHAL_PMU
#define CHAL_PMU

#include "chal_cpu.h"
#include "shared/cos_types.h"
#include "pmu.h"

/* 
 * The code below is for the intel x86 pmu specifically; 
 * Documentation for programming with this feature is 
 * found primarily in the Intel 64 and I-32 Architectures
 * Software Developer's Manual, Volume 4.
 */

/* MSR addresses */
#define MSR_PMC0             193      
#define MSR_PMC1             194      
#define MSR_PMC2             195      
#define MSR_PMC3             196
#define MSR_PERFEVTSELX_BASE 390
#define MSR_PERFEVTSEL0      390
#define MSR_PERFEVTSEL1      391
#define MSR_PERFEVTSEL2      392
#define MSR_PERFEVTSEL3      393
#define MSR_FIXED_CTR_CTRL   909
#define MSR_PERF_GLOBAL_CTRL 911  

/* x86 MSR IA32_PERFEVTSELX Programming Bits */
#define IA32_PERFEVTSELX_UMASK_SHFT 8
#define IA32_PERFEVTSELX_CMASK_SHFT 24
#define IA32_PERFEVTSELX_USR       (1 << 16)
#define IA32_PERFEVTSELX_OS        (1 << 17)
#define IA32_PERFEVTSELX_E         (1 << 18)
#define IA32_PERFEVTSELX_INT       (1 << 20)
#define IA32_PERFEVTSELX_ANYTHD    (1 << 21) /* deprecated on pmu version 4 */
#define IA32_PERFEVTSELX_EN        (1 << 22)
#define IA32_PERFEVTSELX_INV       (1 << 23)

/* IA32_FIXED_CTR_CTRL bits */
#define IA32_FIXED_CTR_CTRL_USR_EN (1 << 0)
#define IA32_FIXED_CTR_CTRL_OS_EN  (1 << 1)

/* reference https://perfmon-events.intel.com/ */
#define IA32_PMU_EVENT_DTLB_MISSES_EVTSEL 0x49
#define IA32_PMU_EVENT_DTLB_MISSES_UMASK  0x0E

#define PMU_NUM_FIXED_PMC 3
#define PMU_NUM_PROGRAMMABLE_PMC 7



static int
fixed_cntr_enable(u8_t cntr)
{
    u32_t perf_global_ctrl_lo, perf_global_ctrl_hi;
    u32_t perf_fixed_ctrl_lo, perf_fixed_ctrl_hi;

    if (cntr >= PMU_NUM_FIXED_PMC) return -EINVAL;

    readmsr(MSR_PERF_GLOBAL_CTRL, &perf_global_ctrl_lo, &perf_global_ctrl_hi);
    readmsr(MSR_FIXED_CTR_CTRL, &perf_fixed_ctrl_lo, &perf_fixed_ctrl_hi);

    /* fixed counter enable is in the high dword */
    perf_global_ctrl_hi |= 1 << cntr;
    /* enable OS and USR mode counting of event, maybe this should be made optional? */
    perf_fixed_ctrl_lo  |= (IA32_FIXED_CTR_CTRL_USR_EN | IA32_FIXED_CTR_CTRL_OS_EN) << (cntr * 4);

    writemsr(MSR_PERF_GLOBAL_CTRL, perf_global_ctrl_lo, perf_global_ctrl_hi);
    writemsr(MSR_FIXED_CTR_CTRL, perf_fixed_ctrl_lo, perf_fixed_ctrl_hi);

    return 0;
}

static int
fixed_cntr_disable(u8_t cntr)
{
    u32_t perf_global_ctrl_lo, perf_global_ctrl_hi;
    u32_t perf_fixed_ctrl_lo, perf_fixed_ctrl_hi;

    if (cntr >= PMU_NUM_FIXED_PMC) return -EINVAL;

    readmsr(MSR_PERF_GLOBAL_CTRL, &perf_global_ctrl_lo, &perf_global_ctrl_hi);
    readmsr(MSR_FIXED_CTR_CTRL, &perf_fixed_ctrl_lo, &perf_fixed_ctrl_hi);

    /* fixed counter enable is in the high dword */
    perf_global_ctrl_hi &= ~(1 << cntr);
    /* enable OS and USR mode counting of event, maybe this should be made optional? */
    perf_fixed_ctrl_lo  &= ~((IA32_FIXED_CTR_CTRL_USR_EN | IA32_FIXED_CTR_CTRL_OS_EN) << (cntr * 4)); 

    writemsr(MSR_PERF_GLOBAL_CTRL, perf_global_ctrl_lo, perf_global_ctrl_hi);
    writemsr(MSR_FIXED_CTR_CTRL, perf_fixed_ctrl_lo, perf_fixed_ctrl_hi);

    return 0;
}

static int
event_cntr_enable(u8_t cntr)
{
    u32_t perf_global_ctrl_lo, perf_global_ctrl_hi;

    if (cntr >= PMU_NUM_PROGRAMMABLE_PMC) return -EINVAL;

    readmsr(MSR_PERF_GLOBAL_CTRL, &perf_global_ctrl_lo, &perf_global_ctrl_hi);
    perf_global_ctrl_lo |= (1ul << cntr);
    writemsr(MSR_PERF_GLOBAL_CTRL, perf_global_ctrl_lo, perf_global_ctrl_hi);

    return 0;
}

static int
event_cntr_disable(u8_t cntr)
{
    u32_t perf_global_ctrl_lo, perf_global_ctrl_hi;

    if (cntr >= PMU_NUM_PROGRAMMABLE_PMC) return -EINVAL;

    readmsr(MSR_PERF_GLOBAL_CTRL, &perf_global_ctrl_lo, &perf_global_ctrl_hi);
    perf_global_ctrl_lo &= ~(1ul << cntr);
    writemsr(MSR_PERF_GLOBAL_CTRL, perf_global_ctrl_lo, perf_global_ctrl_hi);

    return 0;
}

static int
event_cntr_program(u8_t cntr, u8_t evt, u8_t umask)
{
    u32_t perf_evt_sel;

    if (cntr >= PMU_NUM_PROGRAMMABLE_PMC) return -EINVAL;

    /* enable counter; count USR and OS mode events; again, maybe optional? */
    perf_evt_sel = IA32_PERFEVTSELX_USR | IA32_PERFEVTSELX_OS | IA32_PERFEVTSELX_EN; 

    /* set event */
    perf_evt_sel |= evt | (umask << IA32_PERFEVTSELX_UMASK_SHFT);
    writemsr(MSR_PERFEVTSELX_BASE + cntr, perf_evt_sel, 0);

    return 0;
}

/* 
 * Enable hardware performance counters to count
 * instructions, core cycles, and DTLB misses.
 * */
int
chal_pmu_enable_cntrs(void)
{
	int ret = 0;

	/* fixed cntr 0: instruction counter */
	ret = fixed_cntr_enable(0);
	if (ret) goto exit;

	/* fixed cntr 1: cycle counter */
	ret = fixed_cntr_enable(1);
	if (ret) goto exit;

	/* we are using programmable counter 0 to count DTLB misses */
	ret = event_cntr_enable(0);
	if (ret) goto exit;

	ret = event_cntr_program(0, IA32_PMU_EVENT_DTLB_MISSES_EVTSEL, IA32_PMU_EVENT_DTLB_MISSES_UMASK);

	return 0;

exit:
	return ret;

}


#endif
