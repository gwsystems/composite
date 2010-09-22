#ifndef SCHED_TIMING_H
#define SCHED_TIMING_H

#define RUNTIME_SEC (30)
#define REPORT_FREQ (3)		/* freq of reporting in seconds */
#define CHLD_REPORT_FREQ (3)    /* freq of reporting in seconds of the child */
#define TIMER_FREQ 100
#define CYC_PER_USEC 1600
#define USEC_PER_TICK 10000
#define CYC_PER_TICK (CYC_PER_USEC*USEC_PER_TICK)

#endif /* !SCHED_TIMING_H */
