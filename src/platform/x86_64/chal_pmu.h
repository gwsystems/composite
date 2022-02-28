#ifndef CHAL_PMU
#define CHAL_PMU

#include "chal_config.h"


/* x86 MSR IA32_PERF_GLOBAL_CTRL Programming Bits */
#define x86_MSR_GLBL_CTRL_EN_PC0 (1 << 0)
#define x86_MSR_GLBL_CTRL_EN_PC1 (1 << 1)
#define x86_MSR_GLBL_CTRL_EN_PC2 (1 << 2)
#define x86_MSR_GLBL_CTRL_EN_PC3 (1 << 3)
#define x86_MSR_GLBL_CTRL_EN_FC0 (1ul << 32)
#define x86_MSR_GLBL_CTRL_EN_FC1 (1ul << 33)
#define x86_MSR_GLBL_CTRL_EN_FC2 (1ul << 34)


/* x86 MSR IA32_PERFEVTSELX Programming Bits */
#define X86_MSR_EVTSEL_EVTMSK_SFT 8
#define X86_MSR_EVTSEL_CMASK_SFT 24
#define X86_MSR_EVTSEL_USR    (1 << 16)
#define X86_MSR_EVTSEL_OS     (1 << 17)
#define X86_MSR_EVTSEL_E      (1 << 18)
#define X86_MSR_EVTSEL_INT    (1 << 20)
#define X86_MSR_EVTSEL_ANYTHD (1 << 21)
#define X86_MSR_EVTSEL_EN     (1 << 22)
#define X86_MSR_EVTSEL_INV    (1 << 23)

/* MSR addresses */
#define MSR_PERFEVTSEL0      390
#define MSR_PERFEVTSEL1      391
#define MSR_PERFEVTSEL2      392
#define MSR_PERFEVTSEL3      393
#define MSR_FIXED_CTR_CTRL   909
#define MSR_PERF_GLOBAL_CTRL 911
#define MSR_PMC1             193      
#define MSR_PMC2             194      
#define MSR_PMC3             195      
#define MSR_PMC4             196      

static inline void
chal_pmu_init(void)
{
    unsigned long perf_global_ctrl = x86_MSR_GLBL_CTRL_EN_PC0;
    unsigned long perf_fixed_ctr   = 1 || (1 << 1);
    u32_t perf_evt_sel = X86_MSR_EVTSEL_USR | X86_MSR_EVTSEL_OS | X86_MSR_EVTSEL_EN | X86_MSR_EVTSEL_ANYTHD | 0x0208;

    asm volatile ("wrmsr" : : "a"((u32_t)perf_global_ctrl), "d"((u32_t)(perf_global_ctrl >> 32)), "c"(MSR_PERF_GLOBAL_CTRL));
    asm volatile ("wrmsr" : : "a"((u32_t)perf_fixed_ctr), "d"((u32_t)(perf_fixed_ctr >> 32)), "c"(MSR_FIXED_CTR_CTRL));
    asm volatile ("wrmsr" : : "a"(perf_evt_sel), "d"(0), "c"(MSR_PERFEVTSEL0));
}

static inline void
chal_pmu_evtset(u8_t evt)
{

}

#endif