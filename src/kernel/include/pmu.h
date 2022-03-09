#ifndef PMU_H
#define PMU_H 

#include "chal_config.h"

/*
 * TODO
 */

int chal_pmu_fixed_cntr_enable(u8_t cntr);
int chal_pmu_fixed_cntr_disable(u8_t cntr);
int chal_pmu_event_cntr_enable(u8_t cntr);

int chal_pmu_event_cntr_disable(u8_t cntr);
int chal_pmu_event_cntr_program(u8_t cntr, u8_t evt, u8_t umask);

static int
pmu_fixed_cntr_enable(u8_t cntr)
{
    return chal_pmu_fixed_cntr_enable(cntr);
}

static int
pmu_fixed_cntr_disable(u8_t cntr)
{
    return chal_pmu_fixed_cntr_disable(cntr);
}

static int
pmu_event_cntr_enable(u8_t cntr)
{
    return chal_pmu_event_cntr_enable(cntr);
}

static int
pmu_event_cntr_disable(u8_t cntr)
{
    return chal_pmu_event_cntr_disable(cntr);
}

static int
pmu_event_cntr_program(u8_t cntr, u8_t evnt, u8_t umask)
{
    return chal_pmu_event_cntr_program(cntr, evnt, umask);
}

#endif