#ifndef COS_CONFIG_H
#define COS_CONFIG_H

#define CPU_GHZ        1.6
#define CPU_TIMER_FREQ 100 // set in your linux .config

// How long should Composite run before returning to Linux?
#define RUNTIME                10 // seconds
// After how many seconds should schedulers print out their information?
#define SCHED_PRINTOUT_PERIOD  5 
#define COMPONENT_ASSERTIONS   1 // activate assertions in components?

#define LINUX_ON_IDLE          1 // should Linux be activated on Composite idle
#define LINUX_HIGHEST_PRIORITY 1 // should Composite run as highest priority?

// cos kernel settings
#define COS_PRINT_MEASUREMENTS 1
#define COS_PRINT_SCHED_EVENTS 1
#define COS_ASSERTIONS_ACTIVE  1

#endif /* COS_CONFIG_H */
