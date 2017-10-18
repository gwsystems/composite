#ifndef SL_CONSTS
#define SL_CONSTS

#define SL_MIN_PERIOD_US    1000
#define SL_MAX_NUM_THDS     MAX_NUM_THREADS
#define SL_CYCS_DIFF        (1<<14)
#define SL_SCHEDRCV_DEFAULT 0 /* cos_sched_rcv in sl_sched_loop be BLOCKING by DEFAULT. booter/root RCV end-point doesn't block anyway! */

#endif /* SL_CONSTS */
