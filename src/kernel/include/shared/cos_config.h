#ifndef COS_CONFIG_H
#define COS_CONFIG_H

#include "cpu_ghz.h"
#define NUM_CPU                1

#define CPU_TIMER_FREQ 100 // set in your linux .config

#define RUNTIME                3 // seconds

// After how many seconds should schedulers print out their information?
#define SCHED_PRINTOUT_PERIOD  29 
#define COMPONENT_ASSERTIONS   1 // activate assertions in components?

//#define LINUX_ON_IDLE          1 // should Linux be activated on Composite idle

/* 
 * Should Composite run as highest priority?  Should NOT be set if
 * using networking (cnet). 
 */
#define LINUX_HIGHEST_PRIORITY 1

#define INIT_CORE              0 // the CPU that does initialization for Composite
/* Currently Linux runs on the last CPU only. The code includes the
 * following macro assumes this. We might need to assign more cores
 * to Linux later. */
#define LINUX_CORE             (NUM_CPU - 1)
#define NUM_CPU_COS            (NUM_CPU > 1 ? NUM_CPU - 1 : 1) /* how many cores Composite owns */
// cos kernel settings
#define COS_PRINT_MEASUREMENTS 1
#define COS_PRINT_SCHED_EVENTS 1
#define COS_ASSERTIONS_ACTIVE  1

/*** Console and output options ***/
/* 
 * Notes: If you are using composite as high priority and no idle to
 * linux, then the shell output will not appear until the Composite
 * system has exited.  Thus, you will want to make the memory size
 * large enough to buffer _all_ output.  Note that currently
 * COS_PRINT_MEM_SZ should not exceed around (1024*1024*3).
 *
 * If you have COS_PRINT_SHELL, you _will not see output_ unless you
 * run 
 * $~/transfer/print
 * after
 * # make
 * but before the runscript.
 */
/* print out to the shell? */
#define COS_PRINT_SHELL   1
/* how much should we buffer before sending an event to the shell? */
#define COS_PRINT_BUF_SZ  128
/* how large should the shared memory region be that will buffer print data? */
#define COS_PRINT_MEM_SZ  (4096)

/* print out to dmesg? */
/* #define COS_PRINT_DMESG 1 */

#endif /* COS_CONFIG_H */
