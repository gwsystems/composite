#ifndef PMU_H
#define PMU_H 


int chal_pmu_enable_cntrs(void);


static int
pmu_enable_cntrs(void)
{
	return chal_pmu_enable_cntrs();
}


#endif
