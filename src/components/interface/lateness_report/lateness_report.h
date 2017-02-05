#ifndef   	PERIODIC_WAKE_H
#define   	PERIODIC_WAKE_H

int periodic_wake_get_misses(unsigned short int tid);
int periodic_wake_get_deadlines(unsigned short int tid);
long periodic_wake_get_lateness(unsigned short int tid);
long periodic_wake_get_miss_lateness(unsigned short int tid);
int periodic_wake_get_period(unsigned short int tid);

#endif 	    /* !PERIODIC_WAKE_H */
