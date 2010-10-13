#ifndef SCHED_TIMING_H
#define SCHED_TIMING_H

#include <cos_config.h>

#define RUNTIME_SEC      RUNTIME
#define REPORT_FREQ      SCHED_PRINTOUT_PERIOD /* freq of reporting in seconds */
#define CHLD_REPORT_FREQ REPORT_FREQ           /* freq of reporting in seconds of the child */

/* required timing data */
#define CPU_FREQUENCY  (CPU_GHZ*1000000000)
#define CYC_PER_USEC   (CPU_FREQUENCY/1000000)
#define TIMER_FREQ     (CPU_TIMER_FREQ)
#define CYC_PER_TICK   (CPU_FREQUENCY/TIMER_FREQ)
#define MS_PER_TICK    (1000/TIMER_FREQ)
#define USEC_PER_TICK  (1000*MS_PER_TICK)

#endif /* !SCHED_TIMING_H */
