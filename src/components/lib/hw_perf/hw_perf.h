#ifndef HW_PERF_H
#define HW_PERF_H

#include <cos_types.h>

unsigned long hw_perf_cnt_instructions(void);
unsigned long hw_perf_cnt_cycles(void);
unsigned long hw_perf_cnt_dtlb_misses(void);


#endif
